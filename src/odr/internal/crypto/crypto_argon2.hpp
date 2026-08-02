#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace odr::internal::crypto::argon2 {

/// Argon2id [RFC 9106], version 0x13, without secret or associated data.
/// `memory` is in KiB and is rounded down to a multiple of `4 * lanes`. Lanes
/// are computed sequentially. Throws `std::invalid_argument` for parameters
/// outside the ranges the spec allows.
std::string id(std::size_t tag_size, std::string_view password,
               std::string_view salt, std::size_t iterations,
               std::size_t memory, std::size_t lanes);

} // namespace odr::internal::crypto::argon2
