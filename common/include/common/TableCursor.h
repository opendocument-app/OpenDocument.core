#ifndef ODR_TABLECURSOR_H
#define ODR_TABLECURSOR_H

#include <common/TablePosition.h>
#include <cstdint>
#include <list>

namespace odr {

class TableCursor final {
public:
  TableCursor() noexcept;

  void addCol(std::uint32_t repeat = 1) noexcept;
  void addRow(std::uint32_t repeat = 1) noexcept;
  void addCell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
               std::uint32_t repeat = 1) noexcept;

  TablePosition getPosition() const noexcept { return {row, col}; }
  std::uint32_t getRow() const noexcept { return row; }
  std::uint32_t getCol() const noexcept { return col; }

private:
  struct Range {
    std::uint32_t start;
    std::uint32_t end;
  };

  std::uint32_t row;
  std::uint32_t col;
  std::list<std::list<Range>> sparse;

  void handleRowspan() noexcept;
};

} // namespace odr

#endif // ODR_TABLECURSOR_H
