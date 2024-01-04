#ifndef ODR_INTERNAL_PDF_FILE_PARSER_HPP
#define ODR_INTERNAL_PDF_FILE_PARSER_HPP

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <iosfwd>

namespace odr::internal::pdf {

struct Header;
struct IndirectObject;
struct Trailer;
struct Xref;
struct StartXref;
class Entry;

class FileParser {
public:
  explicit FileParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;
  const ObjectParser &parser() const;

  IndirectObject read_indirect_object() const;
  Trailer read_trailer() const;
  Xref read_xref() const;
  StartXref read_start_xref() const;

  void read_header() const;
  Entry read_entry() const;

  void seek_start_xref(std::uint32_t margin = 64) const;

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_FILE_PARSER_HPP
