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

Element *parse_page_or_pages(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document, Element *parent);

Font *parse_font(DocumentParser &parser, const ObjectReference &reference,
                 Document &document) {
  Font *font = document.create_element<Font>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  font->type = Type::font;
  font->object_reference = reference;
  font->object = Object(dictionary);

  if (dictionary.has_key("ToUnicode")) {
    std::string stream =
        parser.read_decoded_stream(dictionary["ToUnicode"].as_reference());
    std::istringstream ss(stream);
    CMapParser cmap_parser(ss);
    font->cmap = cmap_parser.parse_cmap();
  }

  return font;
}

Resources *parse_resources(DocumentParser &parser, const Object &object,
                           Document &document) {
  auto *resources = document.create_element<Resources>();

  Dictionary dictionary = parser.resolve_object_copy(object).as_dictionary();

  resources->type = Type::resources;
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

  annotation->type = Type::annotation;
  annotation->object_reference = reference;
  annotation->object = Object(dictionary);

  return annotation;
}

Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
                 Document &document, Element *parent) {
  Page *page = document.create_element<Page>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  page->type = Type::page;
  page->object_reference = reference;
  page->object = Object(dictionary);
  page->parent = dynamic_cast<Pages *>(parent);
  page->resources = parse_resources(parser, dictionary["Resources"], document);

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
                   Document &document) {
  auto *pages = document.create_element<Pages>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  pages->type = Type::pages;
  pages->object_reference = reference;
  pages->object = Object(dictionary);
  pages->count = dictionary["Count"].as_integer();

  for (const Object &kid : dictionary["Kids"].as_array()) {
    pages->kids.push_back(
        parse_page_or_pages(parser, kid.as_reference(), document, pages));
  }

  return pages;
}

Element *parse_page_or_pages(DocumentParser &parser,
                             const ObjectReference &reference,
                             Document &document, Element *parent) {
  // TODO we are parsing twice
  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const std::string &type = dictionary["Type"].as_string();

  if (type == "Pages") {
    return parse_pages(parser, reference, document);
  }
  if (type == "Page") {
    return parse_page(parser, reference, document, parent);
  }

  throw std::runtime_error("unknown element");
}

Catalog *parse_catalog(DocumentParser &parser, const ObjectReference &reference,
                       Document &document) {
  auto *catalog = document.create_element<Catalog>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const ObjectReference &pages_reference = dictionary["Pages"].as_reference();

  catalog->type = Type::catalog;
  catalog->object_reference = reference;
  catalog->object = Object(dictionary);
  catalog->pages = parse_pages(parser, pages_reference, document);

  return catalog;
}

} // namespace

DocumentParser::DocumentParser(std::istream &in)
    : DocumentParser(in, Logger::null()) {}

DocumentParser::DocumentParser(std::istream &in, Logger &logger)
    : m_parser(in), m_logger{&logger} {}

std::istream &DocumentParser::in() { return m_parser.in(); }

FileParser &DocumentParser::parser() { return m_parser; }

const Xref &DocumentParser::xref() const { return m_xref; }

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
  } else if (entry.is_compressed()) {
    const auto &[stream_id, index] = entry.as_compressed();
    ObjectStream &object_stream = load_object_stream(stream_id);
    if (index >= object_stream.members().size()) {
      throw std::runtime_error("object stream member index out of range");
    }
    if (object_stream.members()[index].first != reference.id) {
      ODR_WARNING(*m_logger, "pdf: object stream "
                                 << stream_id << " member " << index
                                 << " has id "
                                 << object_stream.members()[index].first
                                 << ", expected " << reference.id);
    }
    object.object = object_stream.member_object(index);
  } else {
    ODR_WARNING(*m_logger, "pdf: reference " << reference
                                             << " to freed object, treating "
                                                "as null");
  }

  return m_objects.emplace(reference, std::move(object)).first->second;
}

ObjectStream &
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

  return m_object_streams
      .emplace(stream_id, ObjectStream(std::move(data), n, first))
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
  return m_parser.read_stream(static_cast<std::int32_t>(size));
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

  std::vector<std::uint32_t> field_widths;
  for (const Object &width : dictionary["W"].as_array()) {
    field_widths.push_back(width.as_integer());
  }

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

std::unique_ptr<Document> DocumentParser::parse_document() {
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

  auto document = std::make_unique<Document>();
  document->catalog =
      parse_catalog(*this, (*trailer)["Root"].as_reference(), *document);
  return document;
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
