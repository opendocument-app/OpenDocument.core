#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_cmap_parser.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>
#include <odr/internal/pdf/pdf_filter.hpp>

#include <optional>
#include <ranges>
#include <set>
#include <sstream>

namespace odr::internal::pdf {

namespace {

// Normalize /Rotate to {0, 90, 180, 270}: the spec requires a multiple of 90,
// but real files carry 360 and negatives.
Integer normalize_rotate(Integer rotate) {
  rotate = ((rotate % 360) + 360) % 360;
  return (rotate / 90) * 90;
}

/// The page attributes that are inherited through the `Pages` tree
/// (ISO 32000-1 7.7.3.3, Table 30: exactly `Resources`, `MediaBox`,
/// `CropBox`, `Rotate`). Threaded down the tree instead of walking `Parent`
/// pointers — no cycle risk, no re-reads. Each slot holds the value set by the
/// nearest ancestor that carried it (possibly an indirect reference, resolved
/// lazily at the leaf); a null slot means no ancestor set it.
struct PageAttributes {
  /// Default page size used when no `MediaBox` is present anywhere in the
  /// page tree (US Letter, 612 × 792 pt). The spec requires `MediaBox`, so
  /// this is a lenience for malformed files. (`Object` is not a literal type,
  /// hence `static const` rather than `constexpr`.)
  static const Object &default_media_box() {
    static const auto value =
        Object(Array({Object(Integer(0)), Object(Integer(0)),
                      Object(Integer(612)), Object(Integer(792))}));
    return value;
  }

  Object resources;
  Object media_box;
  Object crop_box;
  Object rotate;

  /// Overlay this node's own inheritable entries from `dictionary`. A
  /// present-but-null entry counts as absent (7.3.9: null is equivalent to
  /// omitting the entry) and leaves the inherited value untouched.
  void overlay(const Dictionary &dictionary) {
    const auto take = [&](Object &slot, const std::string &key) {
      if (dictionary.has_key(key) && !dictionary[key].is_null()) {
        slot = dictionary[key];
      }
    };
    take(resources, "Resources");
    take(media_box, "MediaBox");
    take(crop_box, "CropBox");
    take(rotate, "Rotate");
  }

  /// Resolve the accumulated attributes into final values (7.7.3.4): write the
  /// resolved `media_box`/`crop_box`/`rotate` onto `page` (references
  /// resolved, missing `MediaBox` → US Letter, `CropBox` → `MediaBox`,
  /// `Rotate` normalized) and return the `Resources` object (resolved, or an
  /// empty dictionary if absent) for the caller to parse.
  Object resolve_into(Page &page, DocumentParser &parser,
                      const ObjectReference &reference) const {
    page.media_box = parser.resolve_object_copy(media_box);
    if (!page.media_box.is_array()) {
      ODR_WARNING(parser.logger(),
                  "pdf: page " << reference
                               << " has no /MediaBox, defaulting to US Letter");
      page.media_box = default_media_box();
    }

    page.crop_box = parser.resolve_object_copy(crop_box);
    if (!page.crop_box.is_array()) {
      page.crop_box = page.media_box;
    }

    const Object resolved_rotate = parser.resolve_object_copy(rotate);
    page.rotate = resolved_rotate.is_integer()
                      ? normalize_rotate(resolved_rotate.as_integer())
                      : 0;

    if (resources.is_null()) {
      ODR_WARNING(parser.logger(),
                  "pdf: page "
                      << reference
                      << " has no /Resources, using an empty dictionary");
      return Object(Dictionary());
    }
    return resources;
  }
};

Element *parse_page_or_pages(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document, Pages *parent,
                             const PageAttributes &inherited);

Font *parse_font(DocumentParser &parser, const ObjectReference &reference,
                 Document &document) {
  Font *font = document.create_element<Font>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  font->object_reference = reference;
  font->object = Object(dictionary);

  if (dictionary.has_key("ToUnicode")) {
    std::string stream =
        parser.read_decoded_stream(dictionary["ToUnicode"].as_reference());
    std::istringstream ss(std::move(stream));
    CMapParser cmap_parser(ss);
    font->cmap = cmap_parser.parse_cmap();
  }

  return font;
}

Resources *parse_resources(DocumentParser &parser, const Object &object,
                           Document &document) {
  auto *resources = document.create_element<Resources>();

  Dictionary dictionary = parser.resolve_object_copy(object).as_dictionary();

  resources->object = Object(dictionary);

  if (!dictionary["Font"].is_null()) {
    Dictionary font_table =
        parser.resolve_object_copy(dictionary["Font"]).as_dictionary();
    for (const auto &[key, value] : font_table) {
      resources->font[key] = parse_font(parser, value.as_reference(), document);
    }
  }

  return resources;
}

Annotation *parse_annotation(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document) {
  auto *annotation = document.create_element<Annotation>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  annotation->object_reference = reference;
  annotation->object = Object(dictionary);

  return annotation;
}

Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
                 Document &document, Pages *parent, PageAttributes attributes) {
  Page *page = document.create_element<Page>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  page->object_reference = reference;
  page->object = Object(dictionary);
  page->parent = parent;

  // the page overlays its own inheritable entries, then the accumulated
  // attributes are resolved into the page with Table-30 defaults (7.7.3.4)
  attributes.overlay(dictionary);
  const Object resources = attributes.resolve_into(*page, parser, reference);
  page->resources = parse_resources(parser, resources, document);

  if (dictionary["Contents"].is_reference()) {
    page->contents_reference = {dictionary["Contents"].as_reference()};
  } else {
    for (const Object &e : dictionary["Contents"].as_array()) {
      page->contents_reference.push_back(e.as_reference());
    }
  }

  if (dictionary.has_key("Annots")) {
    // TODO why rvalue not working?
    Array annotations =
        parser.resolve_object_copy(dictionary["Annots"]).as_array();
    for (const Object &annotation : annotations) {
      page->annotations.push_back(
          parse_annotation(parser, annotation.as_reference(), document));
    }
  }

  return page;
}

Pages *parse_pages(DocumentParser &parser, const ObjectReference &reference,
                   Document &document, PageAttributes attributes) {
  auto *pages = document.create_element<Pages>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  pages->object_reference = reference;
  pages->object = Object(dictionary);
  pages->count = dictionary["Count"].as_integer();

  // this node overlays its own inheritable attributes before recursing
  attributes.overlay(dictionary);

  for (const Object &kid : dictionary["Kids"].as_array()) {
    pages->kids.push_back(parse_page_or_pages(parser, kid.as_reference(),
                                              document, pages, attributes));
  }

  return pages;
}

Element *parse_page_or_pages(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document, Pages *parent,
                             const PageAttributes &inherited) {
  // TODO we are parsing twice
  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const std::string &type = dictionary["Type"].as_string();

  if (type == "Pages") {
    return parse_pages(parser, reference, document, inherited);
  }
  if (type == "Page") {
    return parse_page(parser, reference, document, parent, inherited);
  }

  throw std::runtime_error("unknown element");
}

Catalog *parse_catalog(DocumentParser &parser, const ObjectReference &reference,
                       Document &document) {
  auto *catalog = document.create_element<Catalog>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const ObjectReference &pages_reference = dictionary["Pages"].as_reference();

  catalog->object_reference = reference;
  catalog->object = Object(dictionary);
  catalog->pages = parse_pages(parser, pages_reference, document, {});

  return catalog;
}

} // namespace

DocumentParser::DocumentParser(std::unique_ptr<std::istream> in,
                               std::shared_ptr<const Decryptor> decryptor,
                               const Logger &logger)
    : m_stream(std::move(in)), m_parser(*m_stream), m_logger{&logger},
      m_decryptor(std::move(decryptor)) {}

std::istream &DocumentParser::in() { return m_parser.in(); }

FileParser &DocumentParser::parser() { return m_parser; }

const Xref &DocumentParser::xref() const { return m_xref; }

const Logger &DocumentParser::logger() const { return *m_logger; }

const IndirectObject &
DocumentParser::read_object(const ObjectReference &reference) {
  if (const auto it = m_objects.find(reference); it != std::end(m_objects)) {
    return it->second;
  }

  IndirectObject object;
  object.reference = reference;

  const auto entry_it = m_xref.table.find(reference);
  if (entry_it == std::end(m_xref.table)) {
    ODR_WARNING(*m_logger, "pdf: object " << reference
                                          << " not in cross-reference table, "
                                             "treating as null");
  } else if (const Xref::Entry &entry = entry_it->second; entry.is_used()) {
    in().seekg(entry.as_used().position);
    object = parser().read_indirect_object();
    // Decrypt string leaves (7.6.2). Skip the /Encrypt dictionary itself (its
    // strings are the un-decrypted /O,/U,…) — it is read before the decryptor
    // exists, so this is defensive.
    if (m_decryptor != nullptr && m_decryptor->authenticated() &&
        m_encrypt_reference != object.reference) {
      decrypt_strings(object.object, object.reference);
    }
  } else if (entry.is_compressed()) {
    const auto &[stream_id, index] = entry.as_compressed();
    const ObjectStream &members = load_object_stream(stream_id);
    if (index >= members.size()) {
      throw std::runtime_error("object stream member index out of range");
    }
    if (members[index].id != reference.id) {
      ODR_WARNING(*m_logger, "pdf: object stream "
                                 << stream_id << " member " << index
                                 << " has id " << members[index].id
                                 << ", expected " << reference.id);
    }
    object.object = members[index].object;
  } else {
    ODR_WARNING(*m_logger, "pdf: reference " << reference
                                             << " to freed object, treating "
                                                "as null");
  }

  return m_objects.emplace(reference, std::move(object)).first->second;
}

const ObjectStream &
DocumentParser::load_object_stream(const std::uint32_t stream_id) {
  if (const auto it = m_object_streams.find(stream_id);
      it != std::end(m_object_streams)) {
    return it->second;
  }

  const IndirectObject &object = read_object(ObjectReference(stream_id, 0));
  if (!object.has_stream) {
    throw std::runtime_error("object stream " + std::to_string(stream_id) +
                             " has no stream data");
  }
  const Dictionary &dictionary = object.object.as_dictionary();

  std::string data = read_decoded_stream(object);
  const std::uint32_t n = resolve_object_copy(dictionary["N"]).as_integer();
  const std::uint32_t first =
      resolve_object_copy(dictionary["First"]).as_integer();

  std::istringstream in(std::move(data));
  return m_object_streams.emplace(stream_id, parse_object_stream(in, n, first))
      .first->second;
}

std::string
DocumentParser::read_object_stream(const ObjectReference &reference) {
  return read_object_stream(read_object(reference));
}

std::string DocumentParser::read_object_stream(const IndirectObject &object) {
  const Object length = object.object.as_dictionary()["Length"];
  std::uint32_t size = 0;
  if (length.is_integer()) {
    size = length.as_integer();
  } else if (length.is_reference()) {
    size = read_object(length.as_reference()).object.as_integer();
  } else {
    throw std::runtime_error("unknown length property");
  }

  in().seekg(object.stream_position.value());
  std::string raw = m_parser.read_stream(static_cast<std::int32_t>(size));

  // Decrypt before filter decoding (7.6.2). Cross-reference streams are read
  // during the trailer-chain walk, before the decryptor exists, so they are
  // never decrypted (7.5.8.2); object streams are decrypted here as a whole,
  // leaving their members plaintext.
  if (m_decryptor != nullptr && m_decryptor->authenticated()) {
    raw = m_decryptor->decrypt_stream(object.reference, std::move(raw));
  }
  return raw;
}

std::string
DocumentParser::read_decoded_stream(const ObjectReference &reference) {
  return read_decoded_stream(read_object(reference));
}

std::string DocumentParser::read_decoded_stream(const IndirectObject &object) {
  std::string raw = read_object_stream(object);

  const Dictionary &dictionary = object.object.as_dictionary();
  Object filter;
  Object decode_parms;
  if (dictionary.has_key("Filter")) {
    filter = deep_resolve_object_copy(dictionary["Filter"]);
  }
  if (dictionary.has_key("DecodeParms")) {
    decode_parms = deep_resolve_object_copy(dictionary["DecodeParms"]);
  }

  DecodeResult result = decode(filter, decode_parms, std::move(raw));
  if (result.stopped_at_filter.has_value()) {
    throw std::runtime_error("unexpected image filter: " +
                             *result.stopped_at_filter);
  }
  return std::move(result.data);
}

std::pair<Xref, Dictionary>
DocumentParser::read_xref_section(const std::uint32_t position) {
  in().clear();
  in().seekg(position);
  parser().parser().skip_whitespace();

  if (!parser().parser().peek_number()) {
    // classic cross-reference table
    Xref xref = parser().read_xref();
    parser().parser().skip_whitespace();
    Trailer trailer = parser().read_trailer();
    return {std::move(xref), std::move(trailer.dictionary)};
  }

  // cross-reference stream (ISO 32000-1 7.5.8); its dictionary doubles as
  // the trailer dictionary
  IndirectObject object = parser().read_indirect_object();
  const Dictionary &dictionary = object.object.as_dictionary();

  if (!object.has_stream) {
    throw std::runtime_error("cross-reference stream has no stream data");
  }
  if (!dictionary.has_key("Type") || !dictionary["Type"].is_name() ||
      dictionary["Type"].as_name() != "XRef") {
    ODR_WARNING(*m_logger, "pdf: cross-reference stream at "
                               << position << " is not marked /Type /XRef");
  }

  // `/Filter`, `/DecodeParms` and `/Length` are required to be direct in
  // cross-reference streams (7.5.8.2), so no reference resolution here.
  std::string data = read_object_stream(object);
  DecodeResult decoded = decode(
      dictionary.has_key("Filter") ? dictionary["Filter"] : Object(),
      dictionary.has_key("DecodeParms") ? dictionary["DecodeParms"] : Object(),
      std::move(data));
  if (decoded.stopped_at_filter.has_value()) {
    throw std::runtime_error("unexpected image filter in cross-reference "
                             "stream: " +
                             *decoded.stopped_at_filter);
  }

  const Array &widths = dictionary["W"].as_array();
  if (widths.size() != 3) {
    throw std::runtime_error(
        "expected three field widths in cross-reference stream /W");
  }
  const std::array<std::uint32_t, 3> field_widths = {
      static_cast<std::uint32_t>(widths[0].as_integer()),
      static_cast<std::uint32_t>(widths[1].as_integer()),
      static_cast<std::uint32_t>(widths[2].as_integer())};

  std::vector<std::pair<std::uint32_t, std::uint32_t>> subsections;
  if (dictionary.has_key("Index")) {
    const Array &index = dictionary["Index"].as_array();
    for (std::size_t i = 0; i + 1 < index.size(); i += 2) {
      subsections.emplace_back(index[i].as_integer(),
                               index[i + 1].as_integer());
    }
  } else {
    subsections.emplace_back(0, dictionary["Size"].as_integer());
  }

  Xref xref = parse_xref_stream_table(decoded.data, field_widths, subsections);
  return {std::move(xref), dictionary};
}

Dictionary DocumentParser::read_trailer_chain() {
  parser().seek_start_xref();
  const StartXref start_xref = parser().read_start_xref();

  std::optional<Dictionary> trailer;
  std::optional<std::uint32_t> position = start_xref.start;
  std::set<std::uint32_t> visited; // guards against `Prev` cycles

  while (position.has_value() && visited.insert(*position).second) {
    auto [xref, trailer_dict] = read_xref_section(*position);

    // hybrid-reference file (7.5.8.4): the `XRefStm` entries fill in what
    // the classic table leaves absent or marks free, before older sections
    // are appended
    if (trailer_dict.has_key("XRefStm")) {
      auto [stream_xref, stream_dict] =
          read_xref_section(trailer_dict["XRefStm"].as_integer());
      xref.merge_hybrid(stream_xref);
    }

    m_xref.append(xref);

    if (trailer_dict.has_key("Prev")) {
      position = trailer_dict["Prev"].as_integer();
    } else {
      position.reset();
    }

    if (!trailer.has_value()) {
      trailer = std::move(trailer_dict);
    }
  }

  return std::move(trailer).value();
}

Decryptor DocumentParser::create_decryptor(const Dictionary &trailer) {
  if (!trailer.has_key("Encrypt")) {
    throw std::runtime_error("pdf: document is not encrypted");
  }

  // The trailer /ID[0] feeds the R 2-4 key derivation (raw bytes).
  std::string id0;
  if (trailer.has_key("ID") && trailer["ID"].is_array()) {
    const Array &id = trailer["ID"].as_array();
    if (id.size() > 0 && id[0].is_string()) {
      id0 = id[0].as_string();
    }
  }

  // Read the /Encrypt dictionary while the decryptor does not yet exist, so
  // its own strings (/O, /U, …) stay un-decrypted; it is then cached.
  Object encrypt = trailer["Encrypt"];
  if (encrypt.is_reference()) {
    m_encrypt_reference = encrypt.as_reference();
    encrypt = read_object(*m_encrypt_reference).object;
  }
  if (!encrypt.is_dictionary()) {
    throw std::runtime_error("pdf: /Encrypt is not a dictionary");
  }

  std::optional<Decryptor> decryptor =
      Decryptor::create(encrypt.as_dictionary(), id0);
  if (!decryptor.has_value()) {
    throw std::runtime_error("pdf: unsupported encryption");
  }
  return std::move(*decryptor);
}

void DocumentParser::setup_encryption(const Dictionary &trailer,
                                      const std::string &password) {
  Decryptor decryptor = create_decryptor(trailer);
  if (!decryptor.authenticate(password)) {
    ODR_WARNING(*m_logger, "pdf: encrypted document, password not accepted");
  }
  m_decryptor = std::make_shared<const Decryptor>(std::move(decryptor));
}

void DocumentParser::note_encrypt_reference(const Dictionary &trailer) {
  // Preserve the /Encrypt self-skip guard (its /O,/U strings must stay
  // un-decrypted) without re-reading the dictionary.
  if (!trailer.has_key("Encrypt")) {
    return;
  }
  if (const Object &encrypt = trailer["Encrypt"]; encrypt.is_reference()) {
    m_encrypt_reference = encrypt.as_reference();
  }
}

std::shared_ptr<const Decryptor> DocumentParser::decryptor() const {
  if (m_decryptor != nullptr && m_decryptor->authenticated()) {
    return m_decryptor;
  }
  return nullptr;
}

void DocumentParser::decrypt_strings(Object &object,
                                     const ObjectReference &reference) {
  if (object.is_standard_string()) {
    object = Object(StandardString(
        m_decryptor->decrypt_string(reference, object.as_standard_string())));
  } else if (object.is_hex_string()) {
    object = Object(HexString(
        m_decryptor->decrypt_string(reference, object.as_hex_string())));
  } else if (object.is_array()) {
    for (Object &element : object.as_array()) {
      decrypt_strings(element, reference);
    }
  } else if (object.is_dictionary()) {
    for (Object &value : object.as_dictionary() | std::views::values) {
      decrypt_strings(value, reference);
    }
  }
}

bool DocumentParser::encrypted() const { return m_decryptor != nullptr; }

bool DocumentParser::authenticated() const {
  return m_decryptor == nullptr || m_decryptor->authenticated();
}

void DocumentParser::probe_encryption(const std::string &password) {
  const Dictionary trailer = read_trailer_chain();
  if (trailer.has_key("Encrypt")) {
    setup_encryption(trailer, password);
  }
}

std::unique_ptr<Document>
DocumentParser::build_document(const Dictionary &trailer) {
  auto document = std::make_unique<Document>();
  document->catalog =
      parse_catalog(*this, trailer["Root"].as_reference(), *document);
  return document;
}

std::unique_ptr<Document>
DocumentParser::parse_document(const std::string &password) {
  const Dictionary trailer = read_trailer_chain();
  if (m_decryptor) {
    // Decryptor supplied at construction; just guard the /Encrypt dict.
    note_encrypt_reference(trailer);
  } else if (trailer.has_key("Encrypt")) {
    setup_encryption(trailer, password);
  }
  if (!authenticated()) {
    throw std::runtime_error("pdf: document is encrypted, password required");
  }
  return build_document(trailer);
}

void DocumentParser::resolve_object(Object &object) {
  if (object.is_reference()) {
    object = read_object(object.as_reference()).object;
  }
}

void DocumentParser::deep_resolve_object(Object &object) {
  if (object.is_reference()) {
    object = read_object(object.as_reference()).object;
  } else if (object.is_array()) {
    for (Object &e : object.as_array()) {
      deep_resolve_object(e);
    }
  } else if (object.is_dictionary()) {
    for (Object &v : object.as_dictionary() | std::views::values) {
      deep_resolve_object(v);
    }
  }
}

Object DocumentParser::resolve_object_copy(const Object &object) {
  Object result = object;
  resolve_object(result);
  return result;
}

Object DocumentParser::deep_resolve_object_copy(const Object &object) {
  Object result = object;
  deep_resolve_object(result);
  return result;
}

} // namespace odr::internal::pdf
