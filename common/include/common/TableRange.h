#ifndef ODR_COMMON_TABLE_RANGE_H
#define ODR_COMMON_TABLE_RANGE_H

#include <common/TablePosition.h>

namespace odr {
namespace common {

class TableRange {
public:
  TableRange(const TablePosition &from, const TablePosition &to) noexcept;
  explicit TableRange(const std::string &);

  const TablePosition &getFrom() const noexcept { return from; }
  const TablePosition &getTo() const noexcept { return to; }
  std::string toString() const noexcept;

private:
  TablePosition from;
  TablePosition to;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_TABLE_RANGE_H
