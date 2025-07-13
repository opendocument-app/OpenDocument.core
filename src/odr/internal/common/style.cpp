#include <odr/internal/common/style.hpp>

namespace odr::internal {

void ResolvedStyle::override(const ResolvedStyle &other) {
  text_style.override(other.text_style);
  paragraph_style.override(other.paragraph_style);
  table_style.override(other.table_style);
  table_column_style.override(other.table_column_style);
  table_row_style.override(other.table_row_style);
  table_cell_style.override(other.table_cell_style);
  graphic_style.override(other.graphic_style);
}

} // namespace odr::internal
