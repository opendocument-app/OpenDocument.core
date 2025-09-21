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

  [[nodiscard]] std::istream &in() const;
  [[nodiscard]] std::streambuf &sb() const;
  [[nodiscard]] const ObjectParser &parser() const;

  [[nodiscard]] IndirectObject read_indirect_object() const;
  [[nodiscard]] Trailer read_trailer() const;
  [[nodiscard]] Xref read_xref() const;
  [[nodiscard]] StartXref read_start_xref() const;

  [[nodiscard]] std::string read_stream(std::int32_t size) const;

  void read_header() const;
  [[nodiscard]] Entry read_entry() const;

  void seek_start_xref(std::uint32_t margin = 64) const;

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
