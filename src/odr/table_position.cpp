#include <odr/table_position.hpp>

#include <odr/internal/util/hash_util.hpp>

#include <stdexcept>

namespace odr {

std::uint32_t TablePosition::to_column_num(const std::string &string) {
  if (string.empty()) {
    throw std::invalid_argument("s is empty");
  }

  std::uint32_t result = 0;
  for (std::size_t i = 0; i < string.size(); ++i) {
    if (string[i] < 'A' || string[i] > 'Z') {
      throw std::invalid_argument("illegal character in \"" + string + "\"");
    }
    result = result * 26 + (string[i] - 'A' + 1);
  }
  return result - 1;
}

std::uint32_t TablePosition::to_row_num(const std::string &string) {
  return std::stoul(string) - 1;
}

std::string TablePosition::to_column_string(std::uint32_t column) {
  std::string result;

  column += 1;
  do {
    if (const std::uint32_t rem = column % 26; rem == 0) {
      result = 'Z' + result;
      column /= 27;
    } else {
      result = static_cast<char>('A' + rem - 1) + result;
      column /= 26;
    }
  } while (column > 0);

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
