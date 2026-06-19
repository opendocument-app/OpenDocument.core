#include <odr/internal/util/byte_stream_util.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <iostream>

namespace odr::internal::util {

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
    throw std::runtime_error("byte_stream: failed to read from stream");
  }
}

std::uint8_t byte_stream::read_u8(std::istream &in) {
  const auto c = in.rdbuf()->sbumpc();
  if (c == eof) {
    in.setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

std::string byte_stream::read_u8s(std::istream &in, const std::size_t n) {
  std::string result(n, '\0');
  if (const auto m = static_cast<std::streamsize>(n);
      in.rdbuf()->sgetn(result.data(), m) != m) {
    throw std::runtime_error("unexpected stream exhaust");
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
