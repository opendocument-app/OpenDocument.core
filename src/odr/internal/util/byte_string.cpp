#include <odr/internal/util/byte_string.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <stdexcept>

namespace odr::internal::util {

namespace {

void require(const std::string_view data, const std::size_t size) {
  if (data.size() < size) {
    throw std::runtime_error("byte_string: read past end");
  }
}

template <typename T> T read_be(const std::string_view data) {
  require(data, sizeof(T));
  return byte::from_big_endian<T>(data);
}

template <typename T> T read_le(const std::string_view data) {
  require(data, sizeof(T));
  return byte::from_little_endian<T>(data);
}

} // namespace

std::uint8_t byte_string::read_u8(const std::string_view data) {
  require(data, 1);
  return static_cast<std::uint8_t>(data[0]);
}

std::uint16_t byte_string::read_u16_le(const std::string_view data) {
  return read_le<std::uint16_t>(data);
}

std::uint32_t byte_string::read_u32_le(const std::string_view data) {
  return read_le<std::uint32_t>(data);
}

std::uint64_t byte_string::read_u64_le(const std::string_view data) {
  return read_le<std::uint64_t>(data);
}

std::uint16_t byte_string::read_u16_be(const std::string_view data) {
  return read_be<std::uint16_t>(data);
}

std::uint32_t byte_string::read_u32_be(const std::string_view data) {
  return read_be<std::uint32_t>(data);
}

std::uint64_t byte_string::read_u64_be(const std::string_view data) {
  return read_be<std::uint64_t>(data);
}

std::uint32_t byte_string::read_uint_be(const std::string_view data,
                                        const std::size_t size) {
  require(data, size);
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < size; ++i) {
    value = (value << 8) | static_cast<std::uint8_t>(data[i]);
  }
  return value;
}

void byte_string::put_u8(std::string &out, const std::uint8_t value) {
  out += static_cast<char>(value);
}

void byte_string::put_u16_le(std::string &out, const std::uint16_t value) {
  out += static_cast<char>(value & 0xff);
  out += static_cast<char>(value >> 8);
}

void byte_string::put_u32_le(std::string &out, const std::uint32_t value) {
  put_u16_le(out, static_cast<std::uint16_t>(value & 0xffff));
  put_u16_le(out, static_cast<std::uint16_t>(value >> 16));
}

void byte_string::put_u16_be(std::string &out, const std::uint16_t value) {
  out += static_cast<char>(value >> 8);
  out += static_cast<char>(value & 0xff);
}

void byte_string::put_u32_be(std::string &out, const std::uint32_t value) {
  put_u16_be(out, static_cast<std::uint16_t>(value >> 16));
  put_u16_be(out, static_cast<std::uint16_t>(value & 0xffff));
}

void byte_string::write_u16_be(std::string &out, const std::size_t pos,
                               const std::uint16_t value) {
  if (pos + 2 > out.size()) {
    throw std::runtime_error("byte_string: write past end");
  }
  out[pos] = static_cast<char>(value >> 8);
  out[pos + 1] = static_cast<char>(value & 0xff);
}

void byte_string::write_u32_be(std::string &out, const std::size_t pos,
                               const std::uint32_t value) {
  if (pos + 4 > out.size()) {
    throw std::runtime_error("byte_string: write past end");
  }
  out[pos] = static_cast<char>(value >> 24);
  out[pos + 1] = static_cast<char>((value >> 16) & 0xff);
  out[pos + 2] = static_cast<char>((value >> 8) & 0xff);
  out[pos + 3] = static_cast<char>(value & 0xff);
}

} // namespace odr::internal::util
