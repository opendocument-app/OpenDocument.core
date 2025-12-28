#include <odr/table_dimension.hpp>

namespace odr {

TableDimensions::TableDimensions() noexcept = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns) noexcept
    : rows{rows}, columns{columns} {}

} // namespace odr
