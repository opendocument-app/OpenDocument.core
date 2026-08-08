#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace odr::internal::util::byte_string {

// The in-memory counterpart to `byte_stream`. Reads decode from the front of
// the view — its data pointer is the offset, its `size()` the bound — and
// throw `std::runtime_error` when it is too short.

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

/// A bounds-checked cursor over an in-memory byte range. Reads copy the bytes
/// out, so packed structs are read without an unaligned dereference. Does not
/// own the range.
class Reader final {
public:
  explicit Reader(const std::string_view data) noexcept : m_data{data} {}

  template <typename T> void read(T &out) {
    static_assert(std::is_trivially_copyable_v<T>);
    read_bytes(&out, sizeof(T));
  }

  template <typename T> [[nodiscard]] T read() {
    T out{};
    read(out);
    return out;
  }

  void skip(std::size_t count);
  /// Jumps to @p position, which must lie inside the range.
  void seek(std::size_t position);

  [[nodiscard]] std::size_t position() const noexcept { return m_position; }
  [[nodiscard]] std::size_t remaining() const noexcept {
    return m_data.size() - m_position;
  }
  /// The bytes from the cursor to the end; leaves the cursor where it is.
  [[nodiscard]] std::string_view rest() const noexcept {
    return m_data.substr(m_position);
  }

private:
  void read_bytes(void *out, std::size_t size);

  std::string_view m_data;
  std::size_t m_position{0};
};

} // namespace odr::internal::util::byte_string
