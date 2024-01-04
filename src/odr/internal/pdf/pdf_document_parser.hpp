#ifndef ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP

#include <odr/internal/pdf/pdf_file_object.hpp>

#include <memory>

namespace odr::internal::pdf {

struct Document;
class FileParser;

class DocumentParser {
public:
  DocumentParser(FileParser &parser);

  std::istream &in();
  FileParser &parser();
  const Xref &xref() const;

  IndirectObject read_object(const ObjectReference &reference);

  std::unique_ptr<Document> parse_document();

private:
  FileParser *m_parser;
  Xref m_xref;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
