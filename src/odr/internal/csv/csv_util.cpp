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

/// The field counts of every record @p dialect finds in @p text.
struct Scan final {
  std::vector<std::uint32_t> counts;
  bool unterminated{false};
};

Scan scan(const std::string_view text, const Dialect dialect) {
  Scan result;
  RecordReader reader(text, dialect);
  std::vector<std::string> fields;
  while (reader.read(fields)) {
    result.counts.push_back(static_cast<std::uint32_t>(fields.size()));
  }
  result.unterminated = reader.unterminated();
  return result;
}

/// How well @p dialect explains @p text: the field count most records carry,
/// and the share of records carrying it.
struct Score final {
  std::uint32_t columns{0};
  double agreement{0.0};
};

Score score(Scan scan, const bool complete) {
  // a sample cut mid-record says nothing about the record it cut
  if (!complete && !scan.counts.empty()) {
    scan.counts.pop_back();
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

  return {modal->first, static_cast<double>(modal->second) /
                            static_cast<double>(scan.counts.size())};
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
    if (scored.columns < 2) {
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
