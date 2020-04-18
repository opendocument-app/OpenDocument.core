#include <common/TableCursor.h>

namespace odr {
namespace common {

TableCursor::TableCursor() noexcept { sparse_.emplace_back(); }

void TableCursor::addCol(const std::uint32_t repeat) noexcept {
  col_ += repeat;
}

void TableCursor::addRow(const std::uint32_t repeat) noexcept {
  row_ += repeat;
  col_ = 0;
  if (repeat > 1) {
    // TODO assert trivial
    sparse_.clear();
  } else if (repeat == 1) {
    sparse_.pop_front();
  }
  if (sparse_.empty())
    sparse_.emplace_back();
  handleRowspan();
}

void TableCursor::addCell(const std::uint32_t colspan,
                          const std::uint32_t rowspan,
                          const std::uint32_t repeat) noexcept {
  const auto newNextCols = col_ + colspan * repeat;

  // handle rowspan
  auto it = sparse_.begin();
  for (std::uint32_t i = 1; i < rowspan; ++i) {
    if (std::next(it) == sparse_.end())
      sparse_.emplace_back();
    ++it;
    it->emplace_back(Range{col_, newNextCols});
  }

  col_ = newNextCols;
  handleRowspan();
}

void TableCursor::handleRowspan() noexcept {
  auto &s = sparse_.front();
  auto it = s.begin();
  while ((it != s.end()) && (col_ == it->start)) {
    col_ = it->end;
    ++it;
  }
  s.erase(s.begin(), it);
}

} // namespace common
} // namespace odr
