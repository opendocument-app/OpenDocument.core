#ifndef ODR_INTERNAL_COMMON_STYLE_H
#define ODR_INTERNAL_COMMON_STYLE_H

#include <odr/style.h>
#include <optional>

namespace odr::internal::common {

struct ResolvedStyle final {
  std::optional<TextStyle> text_style;
  std::optional<ParagraphStyle> paragraph_style;
  std::optional<TableStyle> table_style;
  std::optional<TableColumnStyle> table_column_style;
  std::optional<TableRowStyle> table_row_style;
  std::optional<TableCellStyle> table_cell_style;
  std::optional<GraphicStyle> graphic_style;

  void override(const ResolvedStyle &other);
  void override(ResolvedStyle &&other);
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_STYLE_H
