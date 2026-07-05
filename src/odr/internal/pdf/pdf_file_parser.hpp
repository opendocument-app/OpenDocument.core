#pragma once

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <cstdint>
#include <iosfwd>
#include <optional>
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

  /// Read the raw bytes of a stream object, the cursor positioned at the start
  /// of the data (just past the `stream` keyword's EOL). With a known `/Length`
  /// pass it as `size`; pass `std::nullopt` when the length is missing or
  /// unresolvable to instead scan forward to the `endstream` keyword. Either
  /// way the cursor is left past the trailing `endobj`.
  [[nodiscard]] std::string
  read_stream(std::optional<std::uint32_t> size = std::nullopt);

  /// Parse all `n` members of a decoded object stream (`/Type /ObjStm`,
  /// ISO 32000-1 7.5.7) from the de-filtered payload `in`: a header of `n`
  /// (id, offset) integer pairs followed by the member objects (bare values —
  /// no `n g obj` wrapper, no stream) at `first + offset`. `n` and `first` are
  /// the `/N` and `/First` dictionary entries.
  [[nodiscard]] ObjectStream read_object_stream(std::uint32_t n,
                                                std::uint32_t first);

  void read_header();
  [[nodiscard]] Entry read_entry();

  void seek_start_xref(std::uint32_t margin = 64);

  /// Decode the entry table of a cross-reference stream (ISO 32000-1 7.5.8.3)
  /// from the already de-filtered `data`. `field_widths` is the `/W` array
  /// (three big-endian byte widths; width 0 means the field defaults: type 1,
  /// other fields 0), `subsections` the `/Index` pairs (first id, count).
  /// Entries of unknown type are treated as absent.
  [[nodiscard]] Xref read_xref_stream_table(
      const std::array<std::uint32_t, 3> &field_widths,
      const std::vector<std::pair<std::uint32_t, std::uint32_t>> &subsections);

  /// Last-resort cross-reference recovery for broken files (missing/garbage
  /// `startxref`, wrong offsets, a damaged chain): forward-scan the whole file
  /// for `n g obj` starts, rebuilding `m_xref` (last definition of an id wins)
  /// and collecting `trailer` dictionaries into `m_trailer`. Then object-stream
  /// members are indexed (`index_object_streams`) and, if no `trailer` supplied
  /// a `/Root`, a `/Type /Catalog` object is searched (`recover_root`). Sets
  /// `m_recovered`. Any object cached from the failed attempt is dropped first.
  std::pair<Xref, Dictionary> recover_xref();

private:
  /// The length-less branch of `read_stream`: scan for the `endstream <ws>
  /// endobj` terminator, leaving the cursor past `endobj`.
  [[nodiscard]] std::string read_stream_scanning();

  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
