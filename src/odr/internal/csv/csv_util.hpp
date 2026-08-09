#pragma once

#include <odr/file.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::csv {

/// The lexical shape of a csv file.
struct Dialect final {
  char separator{','};
  char quote{'"'};
};

/// Reads RFC 4180 records out of already-decoded UTF-8 text.
///
/// Rejects nothing: text ending inside a quoted field still yields its record
/// and sets @ref unterminated. Refusing a file is detection's job.
class RecordReader final {
public:
  RecordReader(std::string_view text, Dialect dialect) noexcept;

  /// Reads the next record into @p fields; `false` once the text is exhausted.
  /// An empty line yields no record, so a trailing newline is not a row.
  bool read(std::vector<std::string> &fields);

  /// Whether the text ran out inside a quoted field.
  [[nodiscard]] bool unterminated() const noexcept;

private:
  std::string_view m_text;
  Dialect m_dialect;
  std::size_t m_position{0};
  bool m_unterminated{false};
};

/// What a probe made of a file's opening bytes.
struct Probe final {
  Dialect dialect;
  /// The field count most records carry.
  std::uint32_t columns{0};
  /// Whether the opening line is Excel's `sep=` directive, which names the
  /// separator and is not data.
  bool separator_directive{false};
  /// Whether this looks like a csv at all — see @ref probe.
  bool is_csv{false};
};

/// Scores @p text — a file's opening bytes decoded to UTF-8 — as a csv and
/// resolves its dialect. @p complete says whether that is the whole file, since
/// a sample cut mid-record says nothing about the record it cut.
///
/// The verdict is a detection heuristic, not a validity rule: two columns
/// minimum, because one column is every line of prose ever written, and no
/// dangling quote in a complete file. A declared csv goes straight to
/// @ref RecordReader and is never asked to pass this.
[[nodiscard]] Probe probe(std::string_view text, bool complete,
                          char quote = '"');

/// Reads @p file's opening bytes, decodes them and scores them. Not a csv when
/// @p encoding cannot be decoded.
[[nodiscard]] Probe probe(const abstract::File &file, TextEncoding encoding,
                          char quote = '"');

} // namespace odr::internal::csv
