#include <odr/internal/util/byte_stream_util.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace odr::internal::util {

namespace {

/// The one failure every read here reports.
[[noreturn]] void throw_exhausted() {
  throw std::runtime_error("byte_stream: unexpected stream exhaust");
}

} // namespace

bool byte_stream::try_read(std::istream &in, char *out,
                           const std::size_t count) {
  if (count == 0) {
    return true;
  }
  if (count >
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    return false;
  }
  in.read(out, static_cast<std::streamsize>(count));
  return static_cast<bool>(in);
}

void byte_stream::read(std::istream &in, char *out, std::size_t count) {
  if (!try_read(in, out, count)) {
    throw_exhausted();
  }
}

void byte_stream::skip(std::istream &in, std::uint64_t count) {
  while (count > 0) {
    // ignore(max) disables the count limit.
    const auto step = static_cast<std::streamsize>(std::min<std::uint64_t>(
        count, std::numeric_limits<std::streamsize>::max() - 1));
    in.ignore(step);
    if (in.gcount() != step) {
      throw_exhausted();
    }
    count -= static_cast<std::uint64_t>(step);
  }
}

std::uint8_t byte_stream::read_u8(std::istream &in) {
  return static_cast<std::uint8_t>(read<char>(in));
}

std::string byte_stream::read_u8s(std::istream &in, const std::uint64_t n) {
  constexpr std::uint64_t chunk_size = 4096;

  std::string result;
  std::array<char, chunk_size> buffer{};
  while (result.size() < n) {
    const auto step =
        static_cast<std::size_t>(std::min(chunk_size, n - result.size()));
    read(in, buffer.data(), step);
    result.append(buffer.data(), step);
  }
  return result;
}

std::uint16_t byte_stream::read_u16_le(std::istream &in) {
  return byte::from_little_endian<std::uint16_t>(read_u8s<2>(in));
}

std::uint32_t byte_stream::read_u32_le(std::istream &in) {
  return byte::from_little_endian<std::uint32_t>(read_u8s<4>(in));
}

std::uint64_t byte_stream::read_u64_le(std::istream &in) {
  return byte::from_little_endian<std::uint64_t>(read_u8s<8>(in));
}

std::uint16_t byte_stream::read_u16_be(std::istream &in) {
  return byte::from_big_endian<std::uint16_t>(read_u8s<2>(in));
}

std::uint32_t byte_stream::read_u32_be(std::istream &in) {
  return byte::from_big_endian<std::uint32_t>(read_u8s<4>(in));
}

std::uint64_t byte_stream::read_u64_be(std::istream &in) {
  return byte::from_big_endian<std::uint64_t>(read_u8s<8>(in));
}

} // namespace odr::internal::util
