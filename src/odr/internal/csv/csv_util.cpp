#include <odr/internal/csv/csv_util.hpp>

#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/encoding/detect.hpp>
#include <odr/internal/encoding/transcode.hpp>

#include <algorithm>
#include <array>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace odr::internal {

namespace {

using csv::Dialect;
using csv::Probe;
using csv::RecordReader;

/// The separators worth trying, in the order they win ties.
constexpr std::array candidate_separators{',', ';', '\t', '|'};

/// Share of records that must agree on a field count — not all of them, a
/// stray ragged row is normal and the parser tolerates it.
constexpr double required_agreement = 0.9;

/// Excel opens a file it exported for a `;` locale with `sep=;`.
std::optional<char> separator_directive(const std::string_view text) {
  constexpr std::string_view prefix = "sep=";
  if (!text.starts_with(prefix) || text.size() < prefix.size() + 2) {
    return std::nullopt;
  }
  const char separator = text[prefix.size()];
  const char after = text[prefix.size() + 1];
  if (after != '\n' && after != '\r') {
    return std::nullopt;
  }
  return separator;
}

/// Share of the fields after the first that must begin with a space before the
/// separator is read as punctuation rather than a delimiter.
constexpr double punctuation_spacing = 0.9;

/// Mean field length, in characters, above which the fields read as prose
/// rather than as values. Only ever consulted together with
/// @ref punctuation_spacing.
constexpr std::size_t prose_field_length = 12;

/// The length of @p field in characters, without the spaces around it. Bytes
/// would read four cjk characters as a sentence.
std::size_t trimmed_length(const std::string_view field) {
  constexpr std::string_view blanks = " \t";
  const std::size_t first = field.find_first_not_of(blanks);
  if (first == std::string_view::npos) {
    return 0;
  }
  const std::string_view trimmed =
      field.substr(first, field.find_last_not_of(blanks) - first + 1);
  // every byte that is not a utf-8 continuation byte opens a character
  return static_cast<std::size_t>(
      std::ranges::count_if(trimmed, [](const char c) {
        return (static_cast<unsigned char>(c) & 0xc0) != 0x80;
      }));
}

/// What a record's fields look like — the evidence that its separator is
/// punctuation rather than a delimiter.
struct Fields final {
  /// Fields after the first of their record - the ones a separator precedes.
  std::size_t following{0};
  /// How many of those begin with a space.
  std::size_t space_led{0};
  /// Every field's length in characters, spaces around it excluded.
  std::size_t total_length{0};
  std::size_t count{0};

  Fields &operator+=(const Fields &other) noexcept {
    following += other.following;
    space_led += other.space_led;
    total_length += other.total_length;
    count += other.count;
    return *this;
  }

  Fields &operator-=(const Fields &other) noexcept {
    following -= other.following;
    space_led -= other.space_led;
    total_length -= other.total_length;
    count -= other.count;
    return *this;
  }
};

Fields inspect(const std::vector<std::string> &fields) {
  Fields result;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    const std::string &field = fields[i];
    result.total_length += trimmed_length(field);
    ++result.count;

    if (i == 0) {
      continue;
    }
    ++result.following;
    if (!field.empty() && field.front() == ' ') {
      ++result.space_led;
    }
  }
  return result;
}

/// What @p dialect finds in @p text: a field count per record, and what those
/// fields look like.
struct Scan final {
  std::vector<std::uint32_t> counts;
  bool unterminated{false};
  Fields fields;
  /// The last record's share of @ref fields, so a truncated one can be taken
  /// back out.
  Fields last_record;
};

Scan scan(const std::string_view text, const Dialect dialect) {
  Scan result;
  RecordReader reader(text, dialect);
  std::vector<std::string> fields;
  while (reader.read(fields)) {
    result.counts.push_back(static_cast<std::uint32_t>(fields.size()));
    result.last_record = inspect(fields);
    result.fields += result.last_record;
  }
  result.unterminated = reader.unterminated();
  return result;
}

/// How well @p dialect explains @p text: the field count most records carry,
/// the share of records carrying it, and whether the separator is doing the
/// work of a delimiter or of punctuation.
struct Score final {
  std::uint32_t columns{0};
  double agreement{0.0};
  std::size_t records{0};
  /// Whether the separator reads as punctuation: every field after it opens
  /// with a space, and the fields are long enough to be sentences. Prose that
  /// puts a comma between clauses is a two column table without this, and a
  /// table is the worse way to be wrong about text.
  bool punctuation{false};
};

Score score(Scan scan, const bool complete) {
  // a sample cut mid-record says nothing about the record it cut
  if (!complete && !scan.counts.empty()) {
    scan.counts.pop_back();
    scan.fields -= scan.last_record;
  }
  if (scan.counts.empty()) {
    return {};
  }

  std::map<std::uint32_t, std::size_t> histogram;
  for (const std::uint32_t count : scan.counts) {
    ++histogram[count];
  }
  const auto modal = std::ranges::max_element(
      histogram, {}, [](const auto &entry) { return entry.second; });

  const bool spaced = scan.fields.following > 0 &&
                      static_cast<double>(scan.fields.space_led) /
                              static_cast<double>(scan.fields.following) >=
                          punctuation_spacing;
  const bool wordy =
      scan.fields.count > 0 &&
      scan.fields.total_length / scan.fields.count >= prose_field_length;

  return {modal->first,
          static_cast<double>(modal->second) /
              static_cast<double>(scan.counts.size()),
          scan.counts.size(), spaced && wordy};
}

} // namespace

csv::RecordReader::RecordReader(const std::string_view text,
                                const Dialect dialect) noexcept
    : m_text{text}, m_dialect{dialect} {}

bool csv::RecordReader::read(std::vector<std::string> &fields) {
  fields.clear();
  std::string field;
  bool quoted = false;
  bool started = false;

  const auto end_field = [&] {
    fields.push_back(std::move(field));
    field.clear();
  };

  for (; m_position < m_text.size(); ++m_position) {
    const char c = m_text[m_position];

    if (quoted) {
      if (c != m_dialect.quote) {
        field.push_back(c);
        continue;
      }
      // a doubled quote is an escaped one and stays inside the field
      if (m_position + 1 < m_text.size() &&
          m_text[m_position + 1] == m_dialect.quote) {
        ++m_position;
        field.push_back(m_dialect.quote);
        continue;
      }
      quoted = false;
      continue;
    }

    if (c == m_dialect.quote) {
      quoted = true;
      started = true;
    } else if (c == m_dialect.separator) {
      end_field();
      started = true;
    } else if (c == '\r') {
      // CRLF, and a lone CR
    } else if (c == '\n') {
      if (!started) {
        continue; // empty line
      }
      end_field();
      ++m_position;
      return true;
    } else {
      field.push_back(c);
      started = true;
    }
  }

  // the caller decides what an unterminated field means
  if (quoted) {
    m_unterminated = true;
  }

  if (!started) {
    return false;
  }
  end_field();
  return true;
}

bool csv::RecordReader::unterminated() const noexcept { return m_unterminated; }

csv::Probe csv::probe(const std::string_view text, const bool complete,
                      const char quote) {
  Probe result;
  result.dialect.quote = quote;

  if (const std::optional<char> declared = separator_directive(text);
      declared.has_value()) {
    result.dialect.separator = *declared;
    result.separator_directive = true;

    const std::size_t line_end = text.find_first_of("\r\n");
    const std::size_t body = text.find_first_not_of("\r\n", line_end);
    const Score scored =
        score(scan(body == std::string_view::npos ? "" : text.substr(body),
                   result.dialect),
              complete);
    result.columns = scored.columns;
    result.is_csv = scored.columns >= 1;
    return result;
  }

  Score best;
  for (const char separator : candidate_separators) {
    const Dialect dialect{.separator = separator, .quote = quote};
    const Scan scanned = scan(text, dialect);

    // a dangling quote in a file we have all of is good evidence of not-csv
    if (complete && scanned.unterminated) {
      continue;
    }

    const Score scored = score(scanned, complete);
    if (scored.columns < 2 || scored.punctuation) {
      continue;
    }
    // one record is a line, not a table, however many separators it holds
    if (scored.records < 2) {
      continue;
    }
    if (scored.agreement > best.agreement ||
        (scored.agreement == best.agreement && scored.columns > best.columns)) {
      best = scored;
      result.dialect = dialect;
    }
  }

  result.columns = best.columns;
  result.is_csv = best.columns >= 2 && best.agreement >= required_agreement;
  return result;
}

bool csv::is_number(std::string_view field) noexcept {
  constexpr std::string_view blanks = " \t";
  const std::size_t first = field.find_first_not_of(blanks);
  if (first == std::string_view::npos) {
    return false;
  }
  field = field.substr(first, field.find_last_not_of(blanks) - first + 1);

  std::size_t i = 0;
  const auto digits = [&] {
    const std::size_t start = i;
    while (i < field.size() && field[i] >= '0' && field[i] <= '9') {
      ++i;
    }
    return i - start;
  };

  if (i < field.size() && (field[i] == '-' || field[i] == '+')) {
    ++i;
  }

  const std::size_t integer_start = i;
  const std::size_t integer_digits = digits();
  if (integer_digits == 0) {
    return false;
  }
  // a leading zero means the digits are an identifier, not a quantity
  if (field[integer_start] == '0' && integer_digits > 1) {
    return false;
  }

  if (i < field.size() && field[i] == '.') {
    ++i;
    if (digits() == 0) {
      return false;
    }
  }

  if (i < field.size() && (field[i] == 'e' || field[i] == 'E')) {
    ++i;
    if (i < field.size() && (field[i] == '-' || field[i] == '+')) {
      ++i;
    }
    if (digits() == 0) {
      return false;
    }
  }

  return i == field.size();
}

csv::Probe csv::probe(const abstract::File &file, const TextEncoding encoding,
                      const char quote) {
  if (!text_encoding_is_decodable(encoding)) {
    return {};
  }

  const std::unique_ptr<std::istream> in = file.stream();
  const std::string bytes = encoding::read_probe(*in);
  const bool complete = bytes.size() < encoding::default_probe_size;
  return probe(encoding::to_utf8(bytes, encoding), complete, quote);
}

} // namespace odr::internal
