#include <internal/common/table_position.h>
#include <internal/util/hash_util.h>
#include <stdexcept>

namespace odr::internal::common {

std::uint32_t TablePosition::to_column_num(const std::string &string) {
  if (string.empty()) {
    throw std::invalid_argument("s is empty");
  }

  std::uint32_t result = 0;
  for (std::size_t i = 0; i < string.size(); ++i) {
    if ((string[i] < 'A') || (string[i] > 'Z')) {
      throw std::invalid_argument("illegal character in \"" + string + "\"");
    }
    result = result * 26 + (string[i] - 'A' + 1);
  }
  return result - 1;
}

std::string TablePosition::to_column_string(std::uint32_t column) {
  std::string result;

  column += 1;
  do {
    const auto rem = column % 26;
    if (rem == 0) {
      result = 'Z' + result;
      column /= 27;
    } else {
      result = (char)('A' + rem - 1) + result;
      column /= 26;
    }
  } while (column > 0);

  return result;
}

TablePosition::TablePosition() noexcept = default;

TablePosition::TablePosition(const std::uint32_t row,
                             const std::uint32_t column) noexcept
    : m_row{row}, m_column{column} {}

TablePosition::TablePosition(const std::string &s) {
  const auto pos = s.find_first_of("0123456789");
  if (pos == std::string::npos) {
    throw std::invalid_argument("malformed table position " + s);
  }
  m_row = std::stoul(s.substr(pos));
  if (m_row <= 0) {
    throw std::invalid_argument("row number needs to be at least 1 " +
                                s.substr(pos));
  }
  --m_row;
  m_column = to_column_num(s.substr(0, pos));
}

bool TablePosition::operator==(const TablePosition &rhs) const {
  return m_row == rhs.m_row && m_column == rhs.m_column;
}

bool TablePosition::operator!=(const TablePosition &rhs) const {
  return m_row != rhs.m_row || m_column != rhs.m_column;
}

std::uint32_t TablePosition::row() const noexcept { return m_row; }

std::uint32_t TablePosition::column() const noexcept { return m_column; }

std::string TablePosition::to_string() const noexcept {
  return to_column_string(m_column) + std::to_string(m_row + 1);
}

} // namespace odr::internal::common

std::size_t std::hash<odr::internal::common::TablePosition>::operator()(
    const odr::internal::common::TablePosition &k) const {
  std::size_t result = 0;
  odr::internal::util::hash::hash_combine(result, k.m_row, k.m_column);
  return result;
}
