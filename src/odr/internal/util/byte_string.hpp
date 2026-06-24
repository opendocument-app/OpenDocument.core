#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace odr::internal::util::byte_string {

// Byte primitives over in-memory buffers, the `std::string`/`std::string_view`
// counterpart to `byte_stream` (which reads from an `std::istream`). Reads take
// a `std::string_view` and decode from its **front** — the view's data pointer
// is the offset, its `size()` is the bound — so a sequential reader advances by
// shrinking the view and a random-access reader passes `view.substr(offset)`.
// Reads throw `std::runtime_error` when the view is too short.

[[nodiscard]] std::uint8_t read_u8(std::string_view data);

[[nodiscard]] std::uint16_t read_u16_le(std::string_view data);
[[nodiscard]] std::uint32_t read_u32_le(std::string_view data);
[[nodiscard]] std::uint64_t read_u64_le(std::string_view data);

[[nodiscard]] std::uint16_t read_u16_be(std::string_view data);
[[nodiscard]] std::uint32_t read_u32_be(std::string_view data);
[[nodiscard]] std::uint64_t read_u64_be(std::string_view data);

/// Big-endian unsigned of @p size bytes (1..4) from the front of @p data.
[[nodiscard]] std::uint32_t read_uint_be(std::string_view data,
                                         std::size_t size);

// Appends, big- and little-endian.

void put_u8(std::string &out, std::uint8_t value);

void put_u16_le(std::string &out, std::uint16_t value);
void put_u32_le(std::string &out, std::uint32_t value);

void put_u16_be(std::string &out, std::uint16_t value);
void put_u32_be(std::string &out, std::uint32_t value);

// In-place big-endian patches at an absolute byte offset; throw when the write
// would run past the end of @p out.

void write_u16_be(std::string &out, std::size_t pos, std::uint16_t value);
void write_u32_be(std::string &out, std::size_t pos, std::uint32_t value);

} // namespace odr::internal::util::byte_string
