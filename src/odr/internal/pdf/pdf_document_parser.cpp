#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

namespace odr::internal::pdf {
namespace {

pdf::Element *parse_page_or_pages(DocumentParser &parser,
                                  const ObjectReference &reference,
                                  Document &document, Element *parent);

pdf::Font *parse_font(DocumentParser &parser, const ObjectReference &reference,
                      Document &document) {
  auto font_unique = std::make_unique<Font>();
  Font *font = font_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  font->type = Type::font;
  font->object_reference = reference;
  font->object = dictionary;

  document.element.push_back(std::move(font_unique));
  return font;
}

pdf::Resources *parse_resources(DocumentParser &parser,
                                const ObjectReference &reference,
                                Document &document) {
  auto resources_unique = std::make_unique<Resources>();
  Resources *resources = resources_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  resources->type = Type::resources;
  resources->object_reference = reference;
  resources->object = dictionary;
  resources->font =
      parse_font(parser, dictionary["Font"].as_reference(), document);

  document.element.push_back(std::move(resources_unique));
  return resources;
}

pdf::Annotation *parse_annotation(DocumentParser &parser,
                                  const ObjectReference &reference,
                                  Document &document) {
  auto annotation_unique = std::make_unique<Annotation>();
  Annotation *annotation = annotation_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  annotation->type = Type::annotation;
  annotation->object_reference = reference;
  annotation->object = dictionary;

  document.element.push_back(std::move(annotation_unique));
  return annotation;
}

pdf::Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
                      Document &document, Element *parent) {
  auto page_unique = std::make_unique<Page>();
  Page *page = page_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  page->type = Type::page;
  page->object_reference = reference;
  page->object = dictionary;
  page->parent = dynamic_cast<Pages *>(parent);
  page->contents_reference = dictionary["Contents"].as_reference();
  page->resources =
      parse_resources(parser, dictionary["Resources"].as_reference(), document);

  for (Object annotation : dictionary["Annots"].as_array()) {
    page->annotations.push_back(
        parse_annotation(parser, annotation.as_reference(), document));
  }

  document.element.push_back(std::move(page_unique));
  return page;
}

pdf::Pages *parse_pages(DocumentParser &parser,
                        const ObjectReference &reference, Document &document) {
  auto pages_unique = std::make_unique<Pages>();
  Pages *pages = pages_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  pages->type = Type::pages;
  pages->object_reference = reference;
  pages->object = dictionary;
  pages->count = dictionary["Count"].as_integer();

  for (const auto &kid : dictionary["Kids"].as_array()) {
    pages->kids.push_back(
        parse_page_or_pages(parser, kid.as_reference(), document, pages));
  }

  document.element.push_back(std::move(pages_unique));
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
  auto catalog_unique = std::make_unique<Catalog>();
  Catalog *catalog = catalog_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const ObjectReference &pages_reference = dictionary["Pages"].as_reference();

  catalog->type = Type::catalog;
  catalog->object_reference = reference;
  catalog->object = dictionary;
  catalog->pages = parse_pages(parser, pages_reference, document);

  document.element.push_back(std::move(catalog_unique));
  return catalog;
}

} // namespace

DocumentParser::DocumentParser(std::istream &in) : m_parser(in) {}

std::istream &DocumentParser::in() const { return m_parser.in(); }

const FileParser &DocumentParser::parser() const { return m_parser; }

const Xref &DocumentParser::xref() const { return m_xref; }

IndirectObject DocumentParser::read_object(const ObjectReference &reference) {
  std::uint32_t position = m_xref.table[reference.first].position;
  in().seekg(position);
  return parser().read_indirect_object();
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
  in().seekg(start_xref.start);

  m_xref = parser().read_xref();
  parser().parser().skip_whitespace();
  Trailer trailer = parser().read_trailer();

  auto document = std::make_unique<Document>();
  document->catalog = parse_catalog(*this, trailer.root_reference, *document);
  return document;
}

} // namespace odr::internal::pdf
