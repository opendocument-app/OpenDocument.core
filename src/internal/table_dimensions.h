#ifndef ODR_INTERNAL_TABLE_DIMENSIONS_H
#define ODR_INTERNAL_TABLE_DIMENSIONS_H

#include <cstdint>

namespace odr::internal {

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr::internal

#endif // ODR_INTERNAL_TABLE_DIMENSIONS_H
