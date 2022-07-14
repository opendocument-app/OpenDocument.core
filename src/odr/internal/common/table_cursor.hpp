#ifndef ODR_INTERNAL_COMMON_TABLE_CURSOR_H
#define ODR_INTERNAL_COMMON_TABLE_CURSOR_H

#include <cstdint>
#include <list>
#include <odr/internal/common/table_position.h>

namespace odr::internal::common {

class TableCursor final {
public:
  TableCursor() noexcept;

  void add_column(std::uint32_t repeat = 1) noexcept;
  void add_row(std::uint32_t repeat = 1) noexcept;
  void add_cell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
                std::uint32_t repeat = 1) noexcept;

  TablePosition position() const noexcept;
  std::uint32_t row() const noexcept;
  std::uint32_t column() const noexcept;

private:
  struct Range {
    std::uint32_t start;
    std::uint32_t end;
  };

  std::uint32_t m_row{0};
  std::uint32_t m_col{0};
  std::list<std::list<Range>> m_sparse;

  void handle_rowspan_() noexcept;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_TABLE_CURSOR_H
