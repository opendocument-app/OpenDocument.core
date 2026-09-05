#pragma once

#include <cstdint>

/// Marks a public declaration as deprecated. Expands to nothing inside the
/// library and its own bindings, which have to keep serving what they
/// deprecate.
#ifdef ODR_INTERNAL_BUILD
#define ODR_DEPRECATED(message)
#else
#define ODR_DEPRECATED(message) [[deprecated(message)]]
#endif

namespace odr {

using ElementIdentifier = std::uint64_t;

static constexpr ElementIdentifier null_element_id{0};

} // namespace odr
