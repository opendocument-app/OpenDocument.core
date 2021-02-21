#ifndef ODR_COMMON_TABLE_CURSOR_H
#define ODR_COMMON_TABLE_CURSOR_H

#include <common/table_position.h>
#include <cstdint>
#include <list>

namespace odr::common {

class TableCursor final {
public:
  TableCursor() noexcept;

  void add_col(std::uint32_t repeat = 1) noexcept;
  void add_row(std::uint32_t repeat = 1) noexcept;
  void add_cell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
                std::uint32_t repeat = 1) noexcept;

  TablePosition position() const noexcept;
  std::uint32_t row() const noexcept;
  std::uint32_t col() const noexcept;

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

} // namespace odr::common

#endif // ODR_COMMON_TABLE_CURSOR_H
