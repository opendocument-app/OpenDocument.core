#pragma once

#include <cstdint>

namespace odr {

/// @brief Represents the dimensions of a table.
struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions() noexcept;
  TableDimensions(std::uint32_t rows, std::uint32_t columns) noexcept;
};

} // namespace odr
