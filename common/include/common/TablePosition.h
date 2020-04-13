#ifndef ODR_COMMON_TABLE_POSITION_H
#define ODR_COMMON_TABLE_POSITION_H

#include <cstdint>
#include <string>

namespace odr {
namespace common {

class TablePosition final {
public:
  static std::uint32_t toColNum(const std::string &string);
  static std::string toColString(std::uint32_t col);

  TablePosition() noexcept;
  TablePosition(std::uint32_t row, std::uint32_t col) noexcept;
  explicit TablePosition(const std::string &);

  std::uint32_t row() const noexcept { return row_; }
  std::uint32_t col() const noexcept { return col_; }
  std::string toString() const noexcept;

private:
  std::uint32_t row_{0};
  std::uint32_t col_{0};
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_TABLE_POSITION_H
