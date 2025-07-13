#pragma once

#include <odr/style.hpp>

#include <optional>

namespace odr::internal {

struct ResolvedStyle final {
  TextStyle text_style;
  ParagraphStyle paragraph_style;
  TableStyle table_style;
  TableColumnStyle table_column_style;
  TableRowStyle table_row_style;
  TableCellStyle table_cell_style;
  GraphicStyle graphic_style;

  void override(const ResolvedStyle &other);
};

} // namespace odr::internal
