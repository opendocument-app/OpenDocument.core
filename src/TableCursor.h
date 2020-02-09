#ifndef ODR_TABLECURSOR_H
#define ODR_TABLECURSOR_H

#include <cstdint>
#include <list>

namespace odr {

class TableCursor final {
public:
    TableCursor() noexcept;

    void addCol(std::uint32_t repeat = 1) noexcept;
    void addRow(std::uint32_t repeat = 1) noexcept;
    void addCell(std::uint32_t colspan, std::uint32_t rowspan, std::uint32_t repeat = 1) noexcept;

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

}

#endif //ODR_TABLECURSOR_H
