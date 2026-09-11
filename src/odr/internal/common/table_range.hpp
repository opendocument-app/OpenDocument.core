#pragma once

#include <odr/table_position.hpp>

#include <string>

namespace odr::internal {

class TableRange final {
public:
  TableRange() noexcept;
  TableRange(const TablePosition &from, const TablePosition &to) noexcept;
  explicit TableRange(const std::string &);

  [[nodiscard]] const TablePosition &from() const noexcept;
  [[nodiscard]] const TablePosition &to() const noexcept;
  [[nodiscard]] std::string to_string() const noexcept;

  /// Closed: @ref to is the last position of the range, and it is contained.
  [[nodiscard]] bool contains(const TablePosition &position) const noexcept;

  friend bool operator==(const TableRange &, const TableRange &) = default;

private:
  TablePosition m_from;
  TablePosition m_to;
};

} // namespace odr::internal
