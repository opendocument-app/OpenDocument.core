#pragma once

#include <cstdint>

namespace odr {

using ElementIdentifier = std::uint64_t;

static constexpr ElementIdentifier null_element_id{0};

/// Which side of an anchor a new element goes on.
enum class Placement : std::uint8_t { before, after };

} // namespace odr
