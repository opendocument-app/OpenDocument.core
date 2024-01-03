#ifndef ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP

#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <memory>

namespace odr::internal::pdf {

struct Document;
class FileParser;

class DocumentParser {
public:
  DocumentParser(FileParser &parser) : m_parser{&parser} {}

  std::istream &in() { return parser().in(); }
  FileParser &parser() { return *m_parser; }
  const Xref &xref() const { return m_xref; }

  IndirectObject read_object(const ObjectReference &reference);

  std::unique_ptr<Document> parse_document();

private:
  FileParser *m_parser;
  Xref m_xref;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
