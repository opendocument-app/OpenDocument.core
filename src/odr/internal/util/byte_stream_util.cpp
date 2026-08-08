#include <odr/internal/util/byte_stream_util.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace odr::internal::util {

namespace {

/// The one failure every read here reports.
[[noreturn]] void throw_exhausted() {
  throw std::runtime_error("byte_stream: unexpected stream exhaust");
}

} // namespace

bool byte_stream::try_read(std::istream &in, char *out, std::size_t count) {
  while (count > 0) {
    in.read(out, static_cast<std::streamsize>(count));
    if (!in) {
      return false;
    }
    const std::streamsize took = in.gcount();
    out += took;
    count -= static_cast<std::size_t>(took);
  }
  return true;
}

void byte_stream::read(std::istream &in, char *out, std::size_t count) {
  if (!try_read(in, out, count)) {
    throw_exhausted();
  }
}

std::uint8_t byte_stream::read_u8(std::istream &in) {
  const auto c = in.rdbuf()->sbumpc();
  if (c == eof) {
    in.setstate(std::ios::eofbit);
    throw_exhausted();
  }
  return static_cast<std::uint8_t>(c);
}

std::string byte_stream::read_u8s(std::istream &in, const std::uint64_t n) {
  constexpr std::uint64_t chunk_size = 4096;

  std::string result;
  while (result.size() < n) {
    const std::size_t offset = result.size();
    const auto step =
        static_cast<std::size_t>(std::min(chunk_size, n - offset));
    result.resize(offset + step);
    read(in, result.data() + offset, step);
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
