#include "TableLocation.h"

namespace odr {

TableLocation::TableLocation() noexcept {
    nextRow = 0;
    nextCol = 0;
    nextSparse.emplace_back();
}

void TableLocation::addCol(const std::uint32_t repeat) noexcept {
    nextCol += repeat;
}

void TableLocation::addRow(const std::uint32_t repeat) noexcept {
    nextRow += repeat;
    nextCol = 0;
    if (repeat > 1) {
        // TODO assert trivial
        nextSparse.clear();
    } else if (repeat == 1) {
        nextSparse.pop_front();
    }
    if (nextSparse.empty()) nextSparse.emplace_back();
    handleRowspan();
}

void TableLocation::addCell(const std::uint32_t colspan, const std::uint32_t rowspan, const std::uint32_t repeat) noexcept {
    const auto newNextCols = nextCol + colspan * repeat;

    // handle rowspan
    auto it = nextSparse.begin();
    for (std::uint32_t i = 1; i < rowspan; ++i) {
        if (std::next(it) == nextSparse.end()) nextSparse.emplace_back();
        ++it;
        it->emplace_back(Range{nextCol, newNextCols});
    }

    nextCol = newNextCols;
    handleRowspan();
}

void TableLocation::handleRowspan() noexcept {
    auto &sparse = nextSparse.front();
    auto it = sparse.begin();
    while ((it != sparse.end()) && (nextCol == it->start)) {
        nextCol = it->end;
        ++it;
    }
    sparse.erase(sparse.begin(), it);
}

}
