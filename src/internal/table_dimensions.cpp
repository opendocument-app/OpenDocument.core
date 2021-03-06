#include <internal/table_dimensions.h>

namespace odr::internal {

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(std::uint32_t rows, std::uint32_t columns)
    : rows{rows}, columns{columns} {}

} // namespace odr::internal
