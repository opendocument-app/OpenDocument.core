#ifndef ODR_COMMON_TABLE_POSITION_H
#define ODR_COMMON_TABLE_POSITION_H

#include <cstdint>
#include <string>

namespace odr::common {

class TablePosition final {
public:
  static std::uint32_t to_col_num(const std::string &string);
  static std::string to_col_string(std::uint32_t col);

  TablePosition() noexcept;
  TablePosition(std::uint32_t row, std::uint32_t col) noexcept;
  explicit TablePosition(const std::string &);

  [[nodiscard]] std::uint32_t row() const noexcept;
  [[nodiscard]] std::uint32_t col() const noexcept;
  [[nodiscard]] std::string to_string() const noexcept;

private:
  std::uint32_t m_row{0};
  std::uint32_t m_col{0};
};

} // namespace odr::common

#endif // ODR_COMMON_TABLE_POSITION_H
