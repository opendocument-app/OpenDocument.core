#pragma once

#include <cstddef>
#include <iostream>
#include <optional>
#include <streambuf>

namespace odr::internal::util::byte_stream {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();
using pos_type = std::streambuf::pos_type;

bool try_read(std::istream &in, char *out, std::size_t count);

template <typename T> void try_read(std::istream &in, std::optional<T> &out) {
  out.emplace();
  if (!try_read(in, reinterpret_cast<char *>(&out), sizeof(T))) {
    out.reset();
  }
}

template <typename T> std::optional<T> try_read(std::istream &in) {
  std::optional<T> out;
  try_read(in, out);
  return out;
}

void read(std::istream &in, char *out, std::size_t count);

template <typename T> void read(std::istream &in, T &out) {
  read(in, reinterpret_cast<char *>(&out), sizeof(T));
}

template <typename T> T read(std::istream &in) {
  T out;
  read(in, out);
  return out;
}

std::uint8_t read_u8(std::istream &in);

template <std::uint32_t N> std::array<char, N> read_u8s(std::istream &in) {
  std::array<char, N> result;
  if (in.rdbuf()->sgetn(result.data(), result.size()) != result.size()) {
    throw std::runtime_error("unexpected stream exhaust");
  }
  return result;
}

std::string read_u8s(std::istream &in, std::size_t n);

std::uint16_t read_u16_le(std::istream &in);
std::uint32_t read_u32_le(std::istream &in);
std::uint64_t read_u64_le(std::istream &in);

std::uint16_t read_u16_be(std::istream &in);
std::uint32_t read_u32_be(std::istream &in);
std::uint64_t read_u64_be(std::istream &in);

} // namespace odr::internal::util::byte_stream
