#include <internal/common/style.h>
#include <odr/style.h>
#include <utility>

namespace odr::internal::common {

namespace {

template <typename T>
void optional_override(const std::optional<T> &from, std::optional<T> &to) {
  if (from) {
    if (!to) {
      to = T();
    }
    to->override(*from);
  }
}

template <typename T>
void optional_override(std::optional<T> &&from, std::optional<T> &to) {
  if (from) {
    if (!to) {
      to = T();
    }
    to->override(std::move(*from));
  }
}

} // namespace

void ResolvedStyle::override(const ResolvedStyle &other) {
  optional_override(other.text_style, text_style);
  optional_override(other.paragraph_style, paragraph_style);
  optional_override(other.table_style, table_style);
  optional_override(other.table_column_style, table_column_style);
  optional_override(other.table_row_style, table_row_style);
  optional_override(other.table_cell_style, table_cell_style);
  optional_override(other.graphic_style, graphic_style);
}

void ResolvedStyle::override(ResolvedStyle &&other) {
  optional_override(std::move(other.text_style), text_style);
  optional_override(std::move(other.paragraph_style), paragraph_style);
  optional_override(std::move(other.table_style), table_style);
  optional_override(std::move(other.table_column_style), table_column_style);
  optional_override(std::move(other.table_row_style), table_row_style);
  optional_override(std::move(other.table_cell_style), table_cell_style);
  optional_override(std::move(other.graphic_style), graphic_style);
}

} // namespace odr::internal::common
