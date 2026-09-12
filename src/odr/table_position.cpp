#include <odr/table_position.hpp>

#include <odr/internal/util/hash_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <limits>
#include <stdexcept>

namespace odr {

namespace {

constexpr std::uint64_t index_limit = std::numeric_limits<std::uint32_t>::max();

} // namespace

/// Bijective base 26, the digits 1-26.
std::optional<std::uint32_t>
TablePosition::try_to_column_num(const std::string_view string) {
  if (string.empty()) {
    return {};
  }
  std::uint64_t result = 0;
  for (const char c : string) {
    const char letter = internal::util::string::to_upper(c);
    if (letter < 'A' || letter > 'Z') {
      return {};
    }
    result = result * 26 + static_cast<std::uint64_t>(letter - 'A' + 1);
    if (result > index_limit) {
      return {};
    }
  }
  return static_cast<std::uint32_t>(result - 1);
}

std::optional<std::uint32_t>
TablePosition::try_to_row_num(const std::string_view string) {
  if (string.empty()) {
    return {};
  }
  std::uint64_t result = 0;
  for (const char c : string) {
    if (!internal::util::string::is_ascii_digit(c)) {
      return {};
    }
    result = result * 10 + static_cast<std::uint64_t>(c - '0');
    if (result > index_limit) {
      return {};
    }
  }
  if (result == 0) {
    return {};
  }
  return static_cast<std::uint32_t>(result - 1);
}

std::uint32_t TablePosition::to_column_num(const std::string &string) {
  if (const std::optional<std::uint32_t> column = try_to_column_num(string);
      column.has_value()) {
    return *column;
  }
  throw std::invalid_argument("no column in \"" + string + "\"");
}

std::uint32_t TablePosition::to_row_num(const std::string &string) {
  if (const std::optional<std::uint32_t> row = try_to_row_num(string);
      row.has_value()) {
    return *row;
  }
  throw std::invalid_argument("no row in \"" + string + "\"");
}

std::string TablePosition::to_column_string(const std::uint32_t column) {
  std::string result;

  // bijective base 26, i.e. digits 1-26 - a remainder of 0 is the `Z` of the
  // previous multiple and borrows from the quotient
  std::uint64_t number = static_cast<std::uint64_t>(column) + 1;
  do {
    if (const std::uint64_t rem = number % 26; rem == 0) {
      result = 'Z' + result;
      number = number / 26 - 1;
    } else {
      result = static_cast<char>('A' + rem - 1) + result;
      number /= 26;
    }
  } while (number > 0);

  return result;
}

std::string TablePosition::to_row_string(const std::uint32_t row) {
  return std::to_string(row + 1);
}

TablePosition::TablePosition(const std::string &s) {
  const auto pos = s.find_first_of("0123456789");
  if (pos == std::string::npos) {
    throw std::invalid_argument("malformed table position " + s);
  }
  row = to_row_num(s.substr(pos));
  column = to_column_num(s.substr(0, pos));
}

bool TablePosition::operator==(const TablePosition &rhs) const {
  return column == rhs.column && row == rhs.row;
}

std::string TablePosition::to_string() const noexcept {
  return to_column_string(column) + to_row_string(row);
}

std::size_t TablePosition::hash() const noexcept {
  std::size_t result = 0;
  internal::util::hash::hash_combine(result, row, column);
  return result;
}

} // namespace odr

std::size_t std::hash<odr::TablePosition>::operator()(
    const odr::TablePosition &k) const noexcept {
  return k.hash();
}
