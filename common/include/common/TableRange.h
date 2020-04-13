#ifndef ODR_COMMON_TABLE_RANGE_H
#define ODR_COMMON_TABLE_RANGE_H

#include <common/TablePosition.h>

namespace odr {
namespace common {

class TableRange final {
public:
  TableRange() noexcept;
  TableRange(const TablePosition &from, const TablePosition &to) noexcept;
  TableRange(const TablePosition &from, std::uint32_t rows,
             std::uint32_t cols) noexcept;
  explicit TableRange(const std::string &);

  const TablePosition &from() const noexcept { return from_; }
  const TablePosition &to() const noexcept { return to_; }
  std::string toString() const noexcept;

  bool contains(const TablePosition &position) const noexcept;

private:
  TablePosition from_;
  TablePosition to_;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_TABLE_RANGE_H
