#include <odr/internal/common/table_cursor.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace odr::internal {

namespace {

std::uint32_t advance(const std::uint32_t position, const std::uint64_t count) {
  if (count == 0 ||
      count > std::numeric_limits<std::uint32_t>::max() - position) {
    throw std::out_of_range("table extent out of range");
  }
  return position + static_cast<std::uint32_t>(count);
}

} // namespace

void TableCursor::add_column(const std::uint32_t repeat) {
  m_column = advance(m_column, repeat);
}

void TableCursor::add_row(const std::uint32_t repeat) {
  m_row = advance(m_row, repeat);
  m_column = 0;
  std::erase_if(m_spans,
                [&](const Range &span) { return span.end_row <= m_row; });
  m_next_span = 0;
  handle_rowspan_();
}

void TableCursor::add_cell(const std::uint32_t colspan,
                           const std::uint32_t rowspan,
                           const std::uint32_t repeat) {
  const std::uint32_t next_column =
      advance(m_column, std::uint64_t{colspan} * repeat);
  const std::uint32_t end_row = advance(m_row, rowspan);
  if (rowspan > 1) {
    const auto position =
        std::ranges::upper_bound(m_spans, m_column, {}, &Range::start);
    const auto index = static_cast<std::size_t>(position - m_spans.begin());
    m_spans.insert(position, Range{m_column, next_column, end_row});
    if (index < m_next_span) {
      ++m_next_span;
    }
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
  while (m_next_span < m_spans.size() &&
         m_spans[m_next_span].start <= m_column) {
    m_column = std::max(m_column, m_spans[m_next_span].end);
    ++m_next_span;
  }
}

} // namespace odr::internal
