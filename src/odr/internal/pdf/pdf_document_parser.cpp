#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_cmap_parser.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>
#include <odr/internal/pdf/pdf_filter.hpp>

#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cctype>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

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

Annotation *parse_annotation(Document &document, const Dictionary &dictionary) {
  auto *annotation = document.create_element<Annotation>();
  annotation->object = Object(dictionary);
  return annotation;
}

Annotation *parse_annotation(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document) {
  IndirectObject object = parser.read_object(reference);
  Annotation *annotation =
      parse_annotation(document, object.object.as_dictionary());
  annotation->object_reference = reference;
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

  // /Contents is a content stream or an array of them, supplied directly or
  // through an indirect reference (7.7.3.3). Resolve a reference first so that
  // a reference to an array is expanded into its stream references rather than
  // mistaken for a single stream.
  const Object &contents = dictionary["Contents"];
  const Object resolved_contents =
      contents.is_reference() ? parser.resolve_object_copy(contents) : contents;
  if (resolved_contents.is_array()) {
    for (const Object &e : resolved_contents.as_array()) {
      page->contents_reference.push_back(e.as_reference());
    }
  } else if (contents.is_reference()) {
    page->contents_reference = {contents.as_reference()};
  }

  if (dictionary.has_key("Annots")) {
    // TODO why rvalue not working?
    Array annotations =
        parser.resolve_object_copy(dictionary["Annots"]).as_array();
    for (const Object &annotation : annotations) {
      // entries are usually indirect references, but inline annotation
      // dictionaries are equally valid (12.5.2)
      if (annotation.is_reference()) {
        page->annotations.push_back(
            parse_annotation(parser, annotation.as_reference(), document));
      } else if (annotation.is_dictionary()) {
        page->annotations.push_back(
            parse_annotation(document, annotation.as_dictionary()));
      }
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
                               std::optional<Decryptor> decryptor,
                               const Logger &logger)
    : m_stream(std::move(in)), m_parser(*m_stream), m_logger{&logger} {
  try {
    auto [xref, trailer] = read_trailer_chain();
    m_xref = std::move(xref);
    m_trailer = std::move(trailer);
  } catch (const std::exception &e) {
    ODR_WARNING(*m_logger, "pdf: cross-reference parsing failed ("
                               << e.what()
                               << "), scanning the file to recover");
    recover_xref();
  }

  if (m_trailer.has_key("Encrypt")) {
    // Build an `Authenticator` from the trailer `/Encrypt` and `/ID`
    // (ISO 32000-1 7.6).

    // The `/Encrypt` dictionary's own `/O`,`/U`,… strings are never encrypted
    // (7.6.2), and need no explicit self-skip guard: it is resolved here while
    // `m_decryptor` is still empty (installed only after this block), so
    // `read_object` leaves its strings raw and caches that copy — every later
    // lookup hits the cache and never re-decrypts it.
    Object encrypt = m_trailer["Encrypt"];
    if (encrypt.is_reference()) {
      encrypt = read_object(encrypt.as_reference()).object;
    }
    if (!encrypt.is_dictionary()) {
      throw std::runtime_error("pdf: /Encrypt is not a dictionary");
    }

    // The trailer /ID[0] feeds the R 2-4 key derivation (raw bytes).
    std::string id0;
    if (m_trailer.has_key("ID") && m_trailer["ID"].is_array()) {
      const Array &id = m_trailer["ID"].as_array();
      if (id.size() > 0 && id[0].is_string()) {
        id0 = id[0].as_string();
      }
    }

    // Set only after resolving `/Encrypt` above: `read_object` throws on an
    // encrypted-but-unauthenticated read, and that resolution runs before any
    // decryptor exists. `m_is_encrypted` records that the file declares
    // encryption regardless of whether we can authenticate it, so an
    // unsupported handler still reports as encrypted.
    m_is_encrypted = true;
    m_authenticator = Authenticator::create(encrypt.as_dictionary(), id0);
  }

  // Install the decryptor only after read_trailer_chain(): cross-reference
  // streams are read during that walk and must stay un-decrypted (7.5.8.2).
  m_decryptor = std::move(decryptor);
}

std::istream &DocumentParser::in() { return m_parser.in(); }

FileParser &DocumentParser::parser() { return m_parser; }

const Logger &DocumentParser::logger() const { return *m_logger; }

const Xref &DocumentParser::xref() const { return m_xref; }

const Dictionary &DocumentParser::trailer() const { return m_trailer; }

bool DocumentParser::is_encrypted() const { return m_is_encrypted; }

bool DocumentParser::is_authenticated() const {
  return m_is_encrypted && m_decryptor.has_value();
}

const std::optional<Authenticator> &DocumentParser::authenticator() const {
  return m_authenticator;
}

const std::optional<Decryptor> &DocumentParser::decryptor() const {
  return m_decryptor;
}

bool DocumentParser::authenticate(const std::string &password) {
  if (m_decryptor.has_value()) {
    throw std::runtime_error("pdf: document is already authenticated");
  }
  if (!m_authenticator.has_value()) {
    throw std::runtime_error("pdf: document is not encrypted");
  }
  m_decryptor = m_authenticator->authenticate(password);
  return m_decryptor.has_value();
}

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
    // Decrypt string leaves (7.6.2). The /Encrypt dictionary needs no special
    // case here: it is read and cached during construction before the
    // decryptor is installed, so its un-decrypted /O,/U,… strings are served
    // from the cache and never reach this path.
    if (is_encrypted() && !is_authenticated()) {
      throw UnauthenticatedReadError();
    }
    if (m_decryptor.has_value()) {
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
  // leaving their members' plaintext.
  if (is_encrypted() && !is_authenticated()) {
    throw UnauthenticatedReadError();
  }
  if (m_decryptor.has_value()) {
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

std::pair<Xref, Dictionary> DocumentParser::read_trailer_chain() {
  parser().seek_start_xref();
  const StartXref start_xref = parser().read_start_xref();

  Xref result_xref;
  std::optional<Dictionary> result_trailer;
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

    result_xref.append(xref);

    if (trailer_dict.has_key("Prev")) {
      position = trailer_dict["Prev"].as_integer();
    } else {
      position.reset();
    }

    if (!result_trailer.has_value()) {
      result_trailer = std::move(trailer_dict);
    }
  }

  return {std::move(result_xref), std::move(result_trailer).value()};
}

namespace {

/// Trim leading and trailing PDF whitespace from `line`, returning the offset
/// of the first non-whitespace byte (so the caller can map back to a file
/// position) and a view of the trimmed content.
std::pair<std::size_t, std::string_view> trim_line(const std::string &line) {
  const std::string_view content =
      util::string::trim_view(line, &ObjectParser::is_whitespace);
  // `content` is a subrange of `line`, so the leading offset is the distance
  // between their data pointers.
  return {static_cast<std::size_t>(content.data() - line.data()), content};
}

/// Recognize an `n g obj` object header at the start of `content` (already
/// trimmed). The dictionary/value may follow on the same line (`12 0 obj<<`),
/// so only the leading `id gen obj` token is required.
std::optional<ObjectReference> match_object_start(std::string_view content) {
  util::stream::ViewStreamBuf buffer(content);
  std::istream stream(&buffer);
  ObjectParser parser(stream);

  // `peek_unsigned_integer` guards each read so a non-matching line is rejected
  // without `read_unsigned_integer` throwing (the common case while scanning).
  if (!parser.peek_unsigned_integer()) {
    return std::nullopt;
  }
  const UnsignedInteger id = parser.read_unsigned_integer();
  if (!parser.peek_whitespace()) {
    return std::nullopt;
  }
  parser.skip_whitespace();
  if (!parser.peek_unsigned_integer()) {
    return std::nullopt;
  }
  const UnsignedInteger gen = parser.read_unsigned_integer();
  if (!parser.peek_whitespace()) {
    return std::nullopt;
  }
  parser.skip_whitespace();

  // the `obj` keyword must follow; guard against identifiers like `object`
  // that merely start with `obj`
  const std::string rest = parser.read_line();
  const std::string_view tail(rest);
  if (!tail.starts_with("obj")) {
    return std::nullopt;
  }
  if (tail.size() > 3 &&
      (std::isalnum(static_cast<unsigned char>(tail[3])) || tail[3] == '.')) {
    return std::nullopt;
  }
  return ObjectReference(id, gen);
}

/// True if `content` (already trimmed) ends with the `stream` keyword on a word
/// boundary. This covers both a bare `stream` line and a compact object that
/// inlines its dictionary and the `stream` token on one line
/// (`N G obj<<...>>stream`). The boundary check rejects `endstream` and
/// identifiers that merely end in `stream`.
bool opens_stream_body(std::string_view content) {
  constexpr std::string_view keyword = "stream";
  if (!content.ends_with(keyword)) {
    return false;
  }
  const std::size_t begin = content.size() - keyword.size();
  return begin == 0 ||
         !std::isalnum(static_cast<unsigned char>(content[begin - 1]));
}

} // namespace

void DocumentParser::recover_xref() {
  // Offsets from the failed attempt may be wrong, so anything cached from it is
  // suspect.
  m_objects.clear();
  m_object_streams.clear();
  m_recovered = true;

  ObjectParser &p = parser().parser();
  std::istream &stream = in();
  stream.clear();
  stream.seekg(0, std::ios::end);
  const auto size = static_cast<std::uint32_t>(stream.tellg());

  Xref xref;
  Dictionary trailer;

  stream.seekg(0);
  while (true) {
    stream.clear(); // drop any eofbit set by the previous read_line
    const std::int64_t tell = stream.tellg();
    if (tell < 0 || static_cast<std::uint32_t>(tell) >= size) {
      break;
    }
    const auto position = static_cast<std::uint32_t>(tell);

    const std::string line = p.read_line();
    const auto [lead, content] = trim_line(line);

    if (std::optional<ObjectReference> ref = match_object_start(content)) {
      // last definition of an id wins (operator[] overwrites)
      xref.table[*ref] = Xref::Entry(
          Xref::UsedEntry{static_cast<std::uint32_t>(position + lead)});
      // A compact object may inline its dictionary and the `stream` token on
      // this same line; fall through to skip the body below. Otherwise the
      // header is fully consumed and we advance to the next line.
      if (!opens_stream_body(content)) {
        continue;
      }
    }

    if (opens_stream_body(content)) {
      // Skip the stream body so its (possibly object-shaped) bytes are not
      // mis-scanned. The length is unknown here, so scan to `endstream`.
      while (true) {
        stream.clear();
        const std::int64_t at = stream.tellg();
        if (at < 0 || static_cast<std::uint32_t>(at) >= size) {
          break;
        }
        if (p.read_line().find("endstream") != std::string::npos) {
          break;
        }
      }
      continue;
    }

    if (content.starts_with("trailer")) {
      const std::int64_t after = stream.tellg(); // start of the next line
      try {
        stream.clear();
        stream.seekg(static_cast<std::int64_t>(position + lead) +
                     7); // "trailer"
        p.skip_whitespace();
        for (const Dictionary dict = p.read_dictionary();
             const auto &[key, value] : dict) {
          trailer[key] = value; // last trailer wins per key
        }
      } catch (const std::exception &) {
        // ignore a malformed trailer and keep scanning
      }
      stream.clear();
      if (after >= 0) {
        stream.seekg(after);
      }
      continue;
    }
  }

  m_xref = std::move(xref);
  m_trailer = std::move(trailer);

  index_object_streams();

  if (!m_trailer.has_key("Root")) {
    recover_root();
  }
}

void DocumentParser::index_object_streams() {
  // Snapshot the directly recovered objects: reading object streams adds
  // compressed entries, which would invalidate an in-flight iterator.
  std::vector<ObjectReference> candidates;
  for (const auto &[reference, entry] : m_xref.table) {
    if (entry.is_used()) {
      candidates.push_back(reference);
    }
  }

  for (const ObjectReference &reference : candidates) {
    try {
      const IndirectObject &object = read_object(reference);
      if (!object.has_stream || !object.object.is_dictionary()) {
        continue;
      }
      const Dictionary &dictionary = object.object.as_dictionary();
      if (!dictionary.has_key("Type") || !dictionary["Type"].is_name() ||
          dictionary["Type"].as_name() != "ObjStm") {
        continue;
      }
      const ObjectStream &members = load_object_stream(reference.id);
      for (std::size_t i = 0; i < members.size(); ++i) {
        // a directly recovered object wins over its compressed copy
        m_xref.table.try_emplace(ObjectReference(members[i].id, 0),
                                 Xref::Entry(Xref::CompressedEntry{
                                     static_cast<std::uint32_t>(reference.id),
                                     static_cast<std::uint32_t>(i)}));
      }
    } catch (const std::exception &) {
      // an unreadable (or, when encrypted, undecryptable) object stream is
      // simply not indexed
    }
  }
}

void DocumentParser::recover_root() {
  for (const auto &[reference, entry] : m_xref.table) {
    if (entry.is_free()) {
      continue;
    }
    try {
      const IndirectObject &object = read_object(reference);
      if (!object.object.is_dictionary()) {
        continue;
      }
      const Dictionary &dictionary = object.object.as_dictionary();
      if (dictionary.has_key("Type") && dictionary["Type"].is_name() &&
          dictionary["Type"].as_name() == "Catalog") {
        ODR_WARNING(*m_logger, "pdf: recovered document catalog " << reference);
        m_trailer["Root"] = Object(reference);
        return;
      }
    } catch (const std::exception &) {
      // skip objects that fail to read during the catalog search
    }
  }
  ODR_WARNING(*m_logger, "pdf: recovery found no document catalog");
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

std::unique_ptr<Document>
DocumentParser::build_document(const Dictionary &trailer) {
  auto document = std::make_unique<Document>();
  document->catalog =
      parse_catalog(*this, trailer["Root"].as_reference(), *document);
  return document;
}

std::unique_ptr<Document> DocumentParser::parse_document() {
  try {
    return build_document(m_trailer);
  } catch (const std::exception &e) {
    // The cross-reference table parsed cleanly but does not describe a usable
    // document (no `/Root`, offsets pointing at the wrong objects, …). Scan the
    // file once and retry; if recovery already ran, give up.
    if (m_recovered) {
      throw;
    }
    ODR_WARNING(*m_logger, "pdf: building the document failed ("
                               << e.what()
                               << "), scanning the file to recover");
    recover_xref();
    return build_document(m_trailer);
  }
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
