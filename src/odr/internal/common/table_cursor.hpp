#pragma once

#include <odr/internal/common/table_position.hpp>

#include <cstdint>
#include <list>

namespace odr::internal {

class TableCursor final {
public:
  TableCursor() noexcept;

  void add_column(std::uint32_t repeat = 1) noexcept;
  void add_row(std::uint32_t repeat = 1) noexcept;
  void add_cell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
                std::uint32_t repeat = 1) noexcept;

  [[nodiscard]] TablePosition position() const noexcept;
  [[nodiscard]] std::uint32_t column() const noexcept;
  [[nodiscard]] std::uint32_t row() const noexcept;

private:
  struct Range {
    std::uint32_t start;
    std::uint32_t end;
  };

  std::uint32_t m_col{0};
  std::uint32_t m_row{0};
  std::list<std::list<Range>> m_sparse;

  void handle_rowspan_() noexcept;
};

} // namespace odr::internal
