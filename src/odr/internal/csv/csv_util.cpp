#include <odr/internal/csv/csv_util.hpp>

#include <cstddef>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal {

namespace {

constexpr char csv_delimiter = ',';
constexpr char csv_quote = '"';

} // namespace

bool csv::read_record(std::istream &in, std::vector<std::string> &fields) {
  fields.clear();
  std::string field;
  bool quoted = false;
  bool started = false;

  const auto end_field = [&] {
    fields.push_back(std::move(field));
    field.clear();
  };

  char c = 0;
  while (in.get(c)) {
    if (quoted) {
      if (c != csv_quote) {
        field.push_back(c);
        continue;
      }
      // a doubled quote is an escaped one and stays inside the field
      if (in.peek() == csv_quote) {
        in.get(c);
        field.push_back(csv_quote);
        continue;
      }
      quoted = false;
      continue;
    }

    switch (c) {
    case csv_quote:
      quoted = true;
      started = true;
      break;
    case csv_delimiter:
      end_field();
      started = true;
      break;
    case '\r':
      break; // CRLF, and a lone CR
    case '\n':
      if (!started) {
        break; // empty line
      }
      end_field();
      return true;
    default:
      field.push_back(c);
      started = true;
      break;
    }
  }

  // reaching EOF inside a quoted field means the quote was never closed; the
  // partial field is not a record
  if (quoted) {
    throw std::runtime_error("csv quoted field is not terminated");
  }

  if (!started) {
    return false;
  }
  end_field();
  return true;
}

void csv::check_csv_file(std::istream &in) {
  std::optional<std::size_t> columns;

  std::vector<std::string> fields;
  while (read_record(in, fields)) {
    if (!columns.has_value()) {
      columns = fields.size();
      continue;
    }
    if (fields.size() != *columns) {
      throw std::runtime_error("csv row has " + std::to_string(fields.size()) +
                               " fields, expected " + std::to_string(*columns));
    }
  }

  // one column is every line of prose ever written, so it is no evidence of a
  // csv; the caller is probing an otherwise unrecognised text file
  if (!columns.has_value() || *columns <= 1) {
    throw std::runtime_error("no csv file");
  }
}

} // namespace odr::internal
