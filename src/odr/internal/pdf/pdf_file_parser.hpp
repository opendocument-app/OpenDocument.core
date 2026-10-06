#pragma once

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>

namespace odr::internal::pdf {

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

  /// Read stream bytes and consume endstream/endobj from the current position.
  /// Without /Length, scan for both terminators (ISO 32000-1 7.3.8.1).
  [[nodiscard]] std::string read_stream(std::uint32_t size);
  [[nodiscard]] std::string read_stream();

  /// Read /N members from a decoded /ObjStm payload with header size /First.
  /// The stream begins at offset zero (ISO 32000-1 7.5.7).
  [[nodiscard]] ObjectStream read_object_stream(std::uint32_t n,
                                                std::uint32_t first);

  void read_header();
  [[nodiscard]] Entry read_entry();

  /// Seek the last startxref line in the trailing search window.
  void seek_start_xref(std::uint32_t margin = 1024);

  /// Decode /W fields and /Index ranges from a decoded cross-reference stream.
  /// Unknown entry types are ignored (ISO 32000-1 7.5.8.3).
  [[nodiscard]] Xref read_xref_stream_table(
      const std::array<std::uint32_t, 3> &field_widths,
      const std::vector<std::pair<std::uint32_t, std::uint32_t>> &subsections);

  /// Scan for cross-reference offsets and trailer entries, skipping stream
  /// bodies.
  std::pair<Xref, Dictionary> recover_xref();

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
