#include <odr/experimental/table_dimensions.h>

namespace odr::experimental {

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(std::uint32_t rows, std::uint32_t columns)
    : rows{rows}, columns{columns} {}

} // namespace odr::experimental
