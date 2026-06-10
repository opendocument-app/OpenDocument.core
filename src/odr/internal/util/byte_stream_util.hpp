#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>

namespace odr::internal::util::byte_stream {

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

} // namespace odr::internal::util::byte_stream
