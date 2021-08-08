#ifndef ODR_INTERNAL_COMMON_TABLE_H
#define ODR_INTERNAL_COMMON_TABLE_H

#include <internal/abstract/table.h>
#include <internal/common/table_range.h>
#include <map>
#include <odr/document.h>

namespace odr::internal::common {

class Table : public abstract::Table {
public:
  [[nodiscard]] TableDimensions dimensions() const override;

  [[nodiscard]] common::TableRange content_bounds() const override;

  [[nodiscard]] ElementIdentifier
  cell_first_child(std::uint32_t row, std::uint32_t column) const override;

  [[nodiscard]] TableDimensions cell_span(std::uint32_t row,
                                          std::uint32_t column) const override;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(std::uint32_t row, std::uint32_t column) const override;

  void update_properties(
      std::uint32_t row, std::uint32_t column,
      std::unordered_map<ElementProperty, std::any> properties) const override;

protected:
  struct Cell final {
    ElementIdentifier first_child;
    std::uint32_t rowspan{1};
    std::uint32_t colspan{1};
  };

  TableDimensions m_dimensions;
  common::TableRange m_content_bounds;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_TABLE_H
