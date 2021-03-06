#include <internal/common/table_range.h>
#include <stdexcept>

namespace odr::internal::common {

TableRange::TableRange() noexcept = default;

TableRange::TableRange(const TablePosition &from,
                       const TablePosition &to) noexcept
    : m_from(from), m_to(to) {}

TableRange::TableRange(const TablePosition &from, std::uint32_t rows,
                       std::uint32_t cols) noexcept
    : m_from(from), m_to{from.row() + rows, from.col() + cols} {}

TableRange::TableRange(const std::string &s) {
  const auto sep = s.find(':');
  if (sep == std::string::npos)
    throw std::invalid_argument("malformed table range " + s);
  m_from = TablePosition(s.substr(0, sep));
  m_to = TablePosition(s.substr(sep + 1));
}

const TablePosition &TableRange::from() const noexcept { return m_from; }

const TablePosition &TableRange::to() const noexcept { return m_to; }

std::string TableRange::to_string() const noexcept {
  return m_from.to_string() + ":" + m_to.to_string();
}

bool TableRange::contains(const TablePosition &position) const noexcept {
  return (m_from.col() <= position.col()) && (m_to.col() > position.col()) &&
         (m_from.row() <= position.row()) && (m_to.row() > position.row());
}

} // namespace odr::internal::common
