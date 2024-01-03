#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

namespace odr::internal::pdf {
namespace {

pdf::Element *parse_element(DocumentParser &parser,
                            const ObjectReference &reference,
                            Document &document, Element *parent);

pdf::Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
                      Document &document, Element *parent) {
  auto page_unique = std::make_unique<Page>();
  Page *page = page_unique.get();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  page->type = Type::page;
  page->object_reference = reference;
  page->parent = dynamic_cast<Pages *>(parent);
  page->contents_reference = dictionary["Contents"].as_reference();

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
  pages->count = dictionary["Count"].as_integer();

  for (const auto &kid : dictionary["Kids"].as_array()) {
    pages->kids.push_back(
        parse_element(parser, kid.as_reference(), document, pages));
  }

  document.element.push_back(std::move(pages_unique));
  return pages;
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
  catalog->pages = parse_pages(parser, pages_reference, document);

  document.element.push_back(std::move(catalog_unique));
  return catalog;
}

pdf::Element *parse_element(DocumentParser &parser,
                            const ObjectReference &reference,
                            Document &document, Element *parent) {
  // TODO we are parsing twice
  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const std::string &type = dictionary["Type"].as_string();

  if (type == "Catalog") {
    return parse_catalog(parser, reference, document);
  }
  if (type == "Pages") {
    return parse_pages(parser, reference, document);
  }
  if (type == "Page") {
    return parse_page(parser, reference, document, parent);
  }

  throw std::runtime_error("unknown element");
}

} // namespace

IndirectObject DocumentParser::read_object(const ObjectReference &reference) {
  std::uint32_t position = m_xref.table[reference.first].position;
  in().seekg(position);
  return parser().read_indirect_object();
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
