#include "TableCursor.h"

namespace odr {

TableCursor::TableCursor() noexcept {
    row = 0;
    col = 0;
    sparse.emplace_back();
}

void TableCursor::addCol(const std::uint32_t repeat) noexcept {
    col += repeat;
}

void TableCursor::addRow(const std::uint32_t repeat) noexcept {
    row += repeat;
    col = 0;
    if (repeat > 1) {
        // TODO assert trivial
        sparse.clear();
    } else if (repeat == 1) {
        sparse.pop_front();
    }
    if (sparse.empty()) sparse.emplace_back();
    handleRowspan();
}

void TableCursor::addCell(const std::uint32_t colspan, const std::uint32_t rowspan, const std::uint32_t repeat) noexcept {
    const auto newNextCols = col + colspan * repeat;

    // handle rowspan
    auto it = sparse.begin();
    for (std::uint32_t i = 1; i < rowspan; ++i) {
        if (std::next(it) == sparse.end()) sparse.emplace_back();
        ++it;
        it->emplace_back(Range{ col, newNextCols});
    }

    col = newNextCols;
    handleRowspan();
}

void TableCursor::handleRowspan() noexcept {
    auto &s = sparse.front();
    auto it = s.begin();
    while ((it != s.end()) && (col == it->start)) {
        col = it->end;
        ++it;
    }
    s.erase(s.begin(), it);
}

}
