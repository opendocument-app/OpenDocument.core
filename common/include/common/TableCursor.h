#ifndef ODR_COMMON_TABLE_CURSOR_H
#define ODR_COMMON_TABLE_CURSOR_H

#include <common/TablePosition.h>
#include <cstdint>
#include <list>

namespace odr {
namespace common {

class TableCursor final {
public:
  TableCursor() noexcept;

  void addCol(std::uint32_t repeat = 1) noexcept;
  void addRow(std::uint32_t repeat = 1) noexcept;
  void addCell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
               std::uint32_t repeat = 1) noexcept;

  TablePosition position() const noexcept { return {row_, col_}; }
  std::uint32_t row() const noexcept { return row_; }
  std::uint32_t col() const noexcept { return col_; }

private:
  struct Range {
    std::uint32_t start;
    std::uint32_t end;
  };

  std::uint32_t row_{0};
  std::uint32_t col_{0};
  std::list<std::list<Range>> sparse_;

  void handleRowspan() noexcept;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_TABLE_CURSOR_H
