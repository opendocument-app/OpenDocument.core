#ifndef ODR_INTERNAL_COMMON_TABLE_POSITION_H
#define ODR_INTERNAL_COMMON_TABLE_POSITION_H

#include <cstdint>
#include <functional>
#include <string>

namespace odr::internal::common {

class TablePosition final {
public:
  static std::uint32_t to_column_num(const std::string &string);
  static std::string to_column_string(std::uint32_t column);

  TablePosition() noexcept;
  TablePosition(std::uint32_t row, std::uint32_t column) noexcept;
  explicit TablePosition(const std::string &);

  bool operator==(const TablePosition &rhs) const;
  bool operator!=(const TablePosition &rhs) const;

  [[nodiscard]] std::uint32_t row() const noexcept;
  [[nodiscard]] std::uint32_t column() const noexcept;
  [[nodiscard]] std::string to_string() const noexcept;

private:
  std::uint32_t m_row{0};
  std::uint32_t m_column{0};

  friend struct std::hash<odr::internal::common::TablePosition>;
};

} // namespace odr::internal::common

template <> struct std::hash<odr::internal::common::TablePosition> {
  std::size_t operator()(const odr::internal::common::TablePosition &k) const;
};

#endif // ODR_INTERNAL_COMMON_TABLE_POSITION_H
