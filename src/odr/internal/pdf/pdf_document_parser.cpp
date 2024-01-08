#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_cmap_parser.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

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
  font->object = dictionary;

  if (dictionary.has_key("ToUnicode")) {
    auto to_unicode_obj =
        parser.read_object(dictionary["ToUnicode"].as_reference());
    std::string stream = parser.read_object_stream(to_unicode_obj);
    std::string inflate = crypto::util::zlib_inflate(stream);
    std::istringstream ss(inflate);
    CMapParser cmap_parser(ss);
    font->cmap = cmap_parser.parse_cmap();
  }

  return font;
}

pdf::Resources *parse_resources(DocumentParser &parser,
                                const ObjectReference &reference,
                                Document &document) {
  Resources *resources = document.create_element<Resources>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  resources->type = Type::resources;
  resources->object_reference = reference;
  resources->object = dictionary;

  if (dictionary["Font"].is_reference()) {
    Dictionary table = parser.read_object(dictionary["Font"].as_reference())
                           .object.as_dictionary();
    for (const auto &[key, value] : table) {
      resources->font[key] = parse_font(parser, value.as_reference(), document);
    }
  } else {
    throw std::runtime_error("problem");
  }

  return resources;
}

pdf::Annotation *parse_annotation(DocumentParser &parser,
                                  const ObjectReference &reference,
                                  Document &document) {
  Annotation *annotation = document.create_element<Annotation>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  annotation->type = Type::annotation;
  annotation->object_reference = reference;
  annotation->object = dictionary;

  return annotation;
}

pdf::Page *parse_page(DocumentParser &parser, const ObjectReference &reference,
                      Document &document, Element *parent) {
  Page *page = document.create_element<Page>();

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

  return page;
}

pdf::Pages *parse_pages(DocumentParser &parser,
                        const ObjectReference &reference, Document &document) {
  Pages *pages = document.create_element<Pages>();

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
  Catalog *catalog = document.create_element<Catalog>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const ObjectReference &pages_reference = dictionary["Pages"].as_reference();

  catalog->type = Type::catalog;
  catalog->object_reference = reference;
  catalog->object = dictionary;
  catalog->pages = parse_pages(parser, pages_reference, document);

  return catalog;
}

} // namespace

DocumentParser::DocumentParser(std::istream &in) : m_parser(in) {}

std::istream &DocumentParser::in() const { return m_parser.in(); }

const FileParser &DocumentParser::parser() const { return m_parser; }

const Xref &DocumentParser::xref() const { return m_xref; }

IndirectObject DocumentParser::read_object(const ObjectReference &reference) {
  std::uint32_t position = m_xref.table[reference.id].position;
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
