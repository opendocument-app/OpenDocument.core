#pragma once

#include <odr/sheet_position.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// Rows inserted into or removed from one sheet.
struct RowEdit final {
  std::string sheet; ///< the `table:name` of the edited sheet
  std::uint32_t row{0};
  std::uint32_t count{0};
  bool insert{true};
};

/// Moves every formula and every cell or range address under @p spreadsheet
/// (`office:spreadsheet`) that names a row of the edited sheet, as
/// `formula::insert_rows` and `formula::delete_rows` do. A formula is written
/// back only where a reference in it moved.
/// @return The formula cells whose result the edit can change, by their
///         position after it: the ones reading a removed row, or a range an
///         insert grows.
[[nodiscard]] std::vector<SheetPosition>
move_row_references(pugi::xml_node spreadsheet, const RowEdit &edit);

} // namespace odr::internal::odf
