#include <common/TablePosition.h>
#include <stdexcept>

namespace odr {
namespace common {

std::uint32_t TablePosition::toColNum(const std::string &s) {
  if (s.empty())
    throw std::invalid_argument("s is empty");

  std::uint32_t result = 0;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if ((s[i] < 'A') || (s[i] > 'Z'))
      throw std::invalid_argument("illegal character in \"" + s + "\"");
    result = result * 26 + (s[i] - 'A' + 1);
  }
  return result - 1;
}

std::string TablePosition::toColString(std::uint32_t col) {
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
    : row_(row), col_(col) {}

TablePosition::TablePosition(const std::string &s) {
  const auto pos = s.find_first_of("0123456789");
  if (pos == std::string::npos)
    throw std::invalid_argument("malformed table position " + s);
  row_ = std::stoul(s.substr(pos));
  if (row_ <= 0)
    throw std::invalid_argument("row number needs to be at least 1 " +
                                s.substr(pos));
  --row_;
  col_ = toColNum(s.substr(0, pos));
}

std::string TablePosition::toString() const noexcept {
  return toColString(col_) + std::to_string(row_ + 1);
}

} // namespace common
} // namespace odr
