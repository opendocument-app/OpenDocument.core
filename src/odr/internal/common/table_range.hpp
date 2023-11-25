#ifndef ODR_INTERNAL_COMMON_TABLE_RANGE_H
#define ODR_INTERNAL_COMMON_TABLE_RANGE_H

#include <odr/internal/common/table_position.hpp>

#include <cstdint>
#include <string>

namespace odr::internal::common {

class TableRange final {
public:
  TableRange() noexcept;
  TableRange(const TablePosition &from, const TablePosition &to) noexcept;
  TableRange(const TablePosition &from, std::uint32_t rows,
             std::uint32_t cols) noexcept;
  explicit TableRange(const std::string &);

  [[nodiscard]] const TablePosition &from() const noexcept;
  [[nodiscard]] const TablePosition &to() const noexcept;
  [[nodiscard]] std::string to_string() const noexcept;

  [[nodiscard]] bool contains(const TablePosition &position) const noexcept;

private:
  TablePosition m_from;
  TablePosition m_to;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_TABLE_RANGE_H
