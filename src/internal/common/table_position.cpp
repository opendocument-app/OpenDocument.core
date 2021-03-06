#include <internal/common/table_position.h>
#include <stdexcept>

namespace odr::internal::common {

std::uint32_t TablePosition::to_col_num(const std::string &string) {
  if (string.empty())
    throw std::invalid_argument("s is empty");

  std::uint32_t result = 0;
  for (std::size_t i = 0; i < string.size(); ++i) {
    if ((string[i] < 'A') || (string[i] > 'Z')) {
      throw std::invalid_argument("illegal character in \"" + string + "\"");
    }
    result = result * 26 + (string[i] - 'A' + 1);
  }
  return result - 1;
}

std::string TablePosition::to_col_string(std::uint32_t col) {
  std::string result;

  col += 1;
  do {
    const auto rem = col % 26;
    if (rem == 0) {
      result = 'Z' + result;
      col /= 27;
    } else {
      result = (char)('A' + rem - 1) + result;
      col /= 26;
    }
  } while (col > 0);

  return result;
}

TablePosition::TablePosition() noexcept = default;

TablePosition::TablePosition(const std::uint32_t row,
                             const std::uint32_t col) noexcept
    : m_row{row}, m_col{col} {}

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
  m_col = to_col_num(s.substr(0, pos));
}

std::uint32_t TablePosition::row() const noexcept { return m_row; }

std::uint32_t TablePosition::col() const noexcept { return m_col; }

std::string TablePosition::to_string() const noexcept {
  return to_col_string(m_col) + std::to_string(m_row + 1);
}

} // namespace odr::internal::common
