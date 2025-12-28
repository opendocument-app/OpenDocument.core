#include <odr/internal/common/table_cursor.hpp>

#include <list>

namespace odr::internal {

TableCursor::TableCursor() noexcept { m_sparse.emplace_back(); }

void TableCursor::add_column(const uint32_t repeat) noexcept {
  m_column += repeat;
}

void TableCursor::add_row(const std::uint32_t repeat) noexcept {
  m_row += repeat;
  m_column = 0;
  if (repeat > 1) {
    // TODO assert trivial
    m_sparse.clear();
  } else if (repeat == 1) {
    m_sparse.pop_front();
  }
  if (m_sparse.empty()) {
    m_sparse.emplace_back();
  }
  handle_rowspan_();
}

void TableCursor::add_cell(const std::uint32_t colspan,
                           const std::uint32_t rowspan,
                           const std::uint32_t repeat) noexcept {
  const std::uint32_t next_column = m_column + colspan * repeat;

  // handle rowspan
  auto it = std::begin(m_sparse);
  for (std::uint32_t i = 1; i < rowspan; ++i) {
    if (std::next(it) == std::end(m_sparse)) {
      m_sparse.emplace_back();
    }
    ++it;
    it->emplace_back(Range{m_column, next_column});
  }

  m_column = next_column;
  handle_rowspan_();
}

TablePosition TableCursor::position() const noexcept {
  return {m_column, m_row};
}

std::uint32_t TableCursor::column() const noexcept { return m_column; }

std::uint32_t TableCursor::row() const noexcept { return m_row; }

void TableCursor::handle_rowspan_() noexcept {
  auto &s = m_sparse.front();
  auto it = std::begin(s);
  while (it != std::end(s) && m_column == it->start) {
    m_column = it->end;
    ++it;
  }
  s.erase(std::begin(s), it);
}

} // namespace odr::internal
