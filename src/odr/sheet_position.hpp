#pragma once

#include <odr/table_position.hpp>

#include <compare>
#include <cstdint>
#include <functional>
#include <string>

namespace odr {

/// A cell of one of a document's sheets: the sheet by its place among them,
/// which is how an edit operation names one, and the cell by position.
struct SheetPosition final {
  std::uint32_t sheet{0};
  TablePosition cell;

  constexpr SheetPosition() noexcept = default;
  constexpr SheetPosition(const std::uint32_t sheet_,
                          const TablePosition &cell_) noexcept
      : sheet{sheet_}, cell{cell_} {}
  constexpr SheetPosition(const std::uint32_t sheet_,
                          const std::uint32_t column,
                          const std::uint32_t row) noexcept
      : sheet{sheet_}, cell{column, row} {}

  bool operator==(const SheetPosition &rhs) const;
  /// The sheet first, then the cell in reading order.
  std::strong_ordering operator<=>(const SheetPosition &rhs) const;

  /// `2!B3`: the sheet's ordinal, then the cell.
  [[nodiscard]] std::string to_string() const noexcept;
  [[nodiscard]] std::size_t hash() const noexcept;
};

} // namespace odr

template <> struct std::hash<odr::SheetPosition> {
  std::size_t operator()(const odr::SheetPosition &k) const noexcept;
};
