#ifndef ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <iosfwd>
#include <memory>

namespace odr::internal::pdf {

struct Document;

class DocumentParser {
public:
  DocumentParser(std::istream &);

  std::istream &in() const;
  const FileParser &parser() const;
  const Xref &xref() const;

  IndirectObject read_object(const ObjectReference &reference);
  std::string read_object_stream(const ObjectReference &reference);
  std::string read_object_stream(const IndirectObject &object);

  std::unique_ptr<Document> parse_document();

private:
  FileParser m_parser;
  Xref m_xref;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_PARSER_HPP
