#pragma once

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

  [[nodiscard]] std::istream &in();
  [[nodiscard]] std::streambuf &sb();
  [[nodiscard]] ObjectParser &parser();

  [[nodiscard]] IndirectObject read_indirect_object();
  [[nodiscard]] Trailer read_trailer();
  [[nodiscard]] Xref read_xref();
  [[nodiscard]] StartXref read_start_xref();

  [[nodiscard]] std::string read_stream(std::int32_t size);

  void read_header();
  [[nodiscard]] Entry read_entry();

  void seek_start_xref(std::uint32_t margin = 64);

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
