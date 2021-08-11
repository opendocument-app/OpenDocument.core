#include <internal/common/table.h>
#include <odr/document.h>

namespace odr::internal::common {

TableDimensions Table::dimensions() const { return m_dimensions; }

common::TableRange Table::content_bounds() const { return m_content_bounds; }

ElementIdentifier
Table::cell_first_child(const std::uint32_t /*row*/,
                        const std::uint32_t /*column*/) const {
  // TODO
  return {};
}

std::unordered_map<ElementProperty, std::any>
Table::properties(std::uint32_t /*row*/, std::uint32_t /*column*/) const {
  // TODO
  return {};
}

void Table::update_properties(
    const std::uint32_t /*row*/, const std::uint32_t /*column*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  // TODO
}

} // namespace odr::internal::common
