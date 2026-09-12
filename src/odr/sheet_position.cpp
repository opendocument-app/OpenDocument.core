#include <odr/sheet_position.hpp>

#include <odr/internal/util/hash_util.hpp>

namespace odr {

bool SheetPosition::operator==(const SheetPosition &rhs) const {
  return sheet == rhs.sheet && cell == rhs.cell;
}

std::strong_ordering
SheetPosition::operator<=>(const SheetPosition &rhs) const {
  if (const std::strong_ordering order = sheet <=> rhs.sheet;
      order != std::strong_ordering::equal) {
    return order;
  }
  if (const std::strong_ordering order = cell.row <=> rhs.cell.row;
      order != std::strong_ordering::equal) {
    return order;
  }
  return cell.column <=> rhs.cell.column;
}

std::string SheetPosition::to_string() const noexcept {
  return std::to_string(sheet) + "!" + cell.to_string();
}

std::size_t SheetPosition::hash() const noexcept {
  std::size_t result = 0;
  internal::util::hash::hash_combine(result, sheet, cell.column, cell.row);
  return result;
}

} // namespace odr

std::size_t std::hash<odr::SheetPosition>::operator()(
    const odr::SheetPosition &k) const noexcept {
  return k.hash();
}
