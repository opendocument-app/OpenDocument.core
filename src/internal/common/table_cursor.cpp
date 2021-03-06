#include <internal/common/table_cursor.h>

namespace odr::internal::common {

TableCursor::TableCursor() noexcept { m_sparse.emplace_back(); }

void TableCursor::add_col(uint32_t repeat) noexcept { m_col += repeat; }

void TableCursor::add_row(uint32_t repeat) noexcept {
  m_row += repeat;
  m_col = 0;
  if (repeat > 1) {
    // TODO assert trivial
    m_sparse.clear();
  } else if (repeat == 1) {
    m_sparse.pop_front();
  }
  if (m_sparse.empty())
    m_sparse.emplace_back();
  handle_rowspan_();
}

void TableCursor::add_cell(uint32_t colspan, uint32_t rowspan,
                           uint32_t repeat) noexcept {
  const auto newNextCols = m_col + colspan * repeat;

  // handle rowspan
  auto it = m_sparse.begin();
  for (std::uint32_t i = 1; i < rowspan; ++i) {
    if (std::next(it) == m_sparse.end())
      m_sparse.emplace_back();
    ++it;
    it->emplace_back(Range{m_col, newNextCols});
  }

  m_col = newNextCols;
  handle_rowspan_();
}

TablePosition TableCursor::position() const noexcept { return {m_row, m_col}; }

std::uint32_t TableCursor::row() const noexcept { return m_row; }

std::uint32_t TableCursor::col() const noexcept { return m_col; }

void TableCursor::handle_rowspan_() noexcept {
  auto &s = m_sparse.front();
  auto it = s.begin();
  while ((it != s.end()) && (m_col == it->start)) {
    m_col = it->end;
    ++it;
  }
  s.erase(s.begin(), it);
}

} // namespace odr::internal::common
