#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_cmap_parser.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <functional>
#include <iostream>
#include <sstream>

namespace odr::internal::pdf {
namespace {

pdf::Element *parse_page_or_pages(DocumentParser &parser,
                                  const ObjectReference &reference,
                                  Document &document, Element *parent);

pdf::Font *parse_font(DocumentParser &parser, const ObjectReference &reference,
                      Document &document) {
  Font *font = document.create_element<Font>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  font->type = Type::font;
  font->object_reference = reference;
  font->object = Object(dictionary);

  if (dictionary.has_key("ToUnicode")) {
    IndirectObject to_unicode_obj =
        parser.read_object(dictionary["ToUnicode"].as_reference());
    std::string stream = parser.read_object_stream(to_unicode_obj);
    std::string inflate = crypto::util::zlib_inflate(stream);
    std::istringstream ss(inflate);
    CMapParser cmap_parser(ss);
    font->cmap = cmap_parser.parse_cmap();
  }

  return font;
}

pdf::Resources *parse_resources(DocumentParser &parser, const Object &object,
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

pdf::Annotation *parse_annotation(DocumentParser &parser,
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

pdf::Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
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

pdf::Pages *parse_pages(DocumentParser &parser,
                        const ObjectReference &reference, Document &document) {
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

pdf::Element *parse_page_or_pages(DocumentParser &parser,
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

pdf::Catalog *parse_catalog(DocumentParser &parser,
                            const ObjectReference &reference,
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

DocumentParser::DocumentParser(std::istream &in) : m_parser(in) {}

std::istream &DocumentParser::in() const { return m_parser.in(); }

const FileParser &DocumentParser::parser() const { return m_parser; }

const Xref &DocumentParser::xref() const { return m_xref; }

const IndirectObject &
DocumentParser::read_object(const ObjectReference &reference) {
  if (auto it = m_objects.find(reference); it != std::end(m_objects)) {
    return it->second;
  }

  std::uint32_t position = m_xref.table.at(reference).position;
  in().seekg(position);
  IndirectObject object = parser().read_indirect_object();

  return m_objects.emplace(reference, std::move(object)).first->second;
}

std::string
DocumentParser::read_object_stream(const ObjectReference &reference) {
  return read_object_stream(read_object(reference));
}

std::string DocumentParser::read_object_stream(const IndirectObject &object) {
  Object length = object.object.as_dictionary()["Length"];
  std::uint32_t size;
  if (length.is_integer()) {
    size = length.as_integer();
  } else if (length.is_reference()) {
    size = read_object(length.as_reference()).object.as_integer();
  } else {
    throw std::runtime_error("unknown length property");
  }

  in().seekg(object.stream_position.value());
  return m_parser.read_stream(size);
}

std::unique_ptr<Document> DocumentParser::parse_document() {
  parser().seek_start_xref();
  StartXref start_xref = parser().read_start_xref();

  std::uint32_t xref_position = start_xref.start;
  std::optional<Trailer> trailer;

  while (true) {
    in().seekg(xref_position);

    m_xref.append(parser().read_xref());
    parser().parser().skip_whitespace();
    Trailer new_trailer = parser().read_trailer();
    if (!trailer) {
      trailer = new_trailer;
    }

    if (new_trailer.dictionary.has_key("Prev")) {
      xref_position = new_trailer.dictionary["Prev"].as_integer();
      continue;
    }

    break;
  }

  auto document = std::make_unique<Document>();
  document->catalog =
      parse_catalog(*this, trailer->root_reference(), *document);
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
    for (auto &[k, v] : object.as_dictionary()) {
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
