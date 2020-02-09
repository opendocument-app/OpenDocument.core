#ifndef ODR_TABLERANGE_H
#define ODR_TABLERANGE_H

#include "TablePosition.h"

namespace odr {

class TableRange {
public:
    TableRange(const TablePosition &from, const TablePosition &to) noexcept;
    explicit TableRange(const std::string &);

    const TablePosition &getFrom() const noexcept { return from; }
    const TablePosition &getTo() const noexcept { return to; }
    std::string toString() const noexcept;

private:
    TablePosition from;
    TablePosition to;
};

}

#endif //ODR_TABLERANGE_H
