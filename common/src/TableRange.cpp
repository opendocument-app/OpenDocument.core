#include <common/TableRange.h>
#include <stdexcept>

namespace odr {
namespace common {

TableRange::TableRange() noexcept = default;

TableRange::TableRange(const TablePosition &from,
                       const TablePosition &to) noexcept
    : from_(from), to_(to) {}

TableRange::TableRange(const TablePosition &from, std::uint32_t rows,
                       std::uint32_t cols) noexcept
    : from_(from), to_{from.row() + rows, from.col() + cols} {}

TableRange::TableRange(const std::string &s) {
  const auto sep = s.find(':');
  if (sep == std::string::npos)
    throw std::invalid_argument("malformed table range " + s);
  from_ = TablePosition(s.substr(0, sep));
  to_ = TablePosition(s.substr(sep + 1));
}

std::string TableRange::toString() const noexcept {
  return from_.toString() + ":" + to_.toString();
}

bool TableRange::contains(const TablePosition &position) const noexcept {
  return (from_.col() <= position.col()) && (to_.col() > position.col()) &&
         (from_.row() <= position.row()) && (to_.row() > position.row());
}

} // namespace common
} // namespace odr
