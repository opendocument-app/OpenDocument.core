#pragma once

#include <odr/table_position.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace odr::internal {

class TableCursor final {
public:
  void add_column(std::uint32_t repeat = 1);
  void add_row(std::uint32_t repeat = 1);
  void add_cell(std::uint32_t colspan = 1, std::uint32_t rowspan = 1,
                std::uint32_t repeat = 1);

  [[nodiscard]] TablePosition position() const noexcept;
  [[nodiscard]] std::uint32_t column() const noexcept;
  [[nodiscard]] std::uint32_t row() const noexcept;

private:
  struct Range {
    std::uint32_t start{0};
    std::uint32_t end{0};
    std::uint32_t end_row{0};
  };

  std::uint32_t m_column{0};
  std::uint32_t m_row{0};
  std::vector<Range> m_spans;
  std::size_t m_next_span{0};

  void handle_rowspan_() noexcept;
};

} // namespace odr::internal
