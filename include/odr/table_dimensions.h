#ifndef ODR_TABLE_DIMENSIONS_H
#define ODR_TABLE_DIMENSIONS_H

#include <cstdint>

namespace odr {

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_TABLE_DIMENSIONS_H
