#include <common/TableRange.h>
#include <stdexcept>

namespace odr {

TableRange::TableRange(const TablePosition &from,
                       const TablePosition &to) noexcept
    : from(from), to(to) {}

TableRange::TableRange(const std::string &s) {
  const auto sep = s.find(':');
  if (sep == std::string::npos)
    throw std::invalid_argument("malformed table range " + s);
  from = TablePosition(s.substr(0, sep));
  to = TablePosition(s.substr(sep + 1));
}

std::string TableRange::toString() const noexcept {
  return from.toString() + ":" + to.toString();
}

} // namespace odr
