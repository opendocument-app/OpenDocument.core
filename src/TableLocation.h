#ifndef ODR_TABLELOCATION_H
#define ODR_TABLELOCATION_H

#include <cstdint>
#include <list>

namespace odr {

class TableLocation final {
public:
    TableLocation() noexcept;

    void addCol(std::uint32_t repeat = 1) noexcept;
    void addRow(std::uint32_t repeat = 1) noexcept;
    void addCell(std::uint32_t colspan, std::uint32_t rowspan, std::uint32_t repeat = 1) noexcept;

    std::uint32_t getNextRow() const noexcept { return nextRow; }
    std::uint32_t getNextCol() const noexcept { return nextCol; }

private:
    struct Range {
        std::uint32_t start;
        std::uint32_t end;
    };

    std::uint32_t nextRow;
    std::uint32_t nextCol;
    std::list<std::list<Range>> nextSparse;

    void handleRowspan() noexcept;
};

}

#endif //ODR_TABLELOCATION_H
