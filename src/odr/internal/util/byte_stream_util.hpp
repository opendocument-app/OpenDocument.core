#pragma once

#include <iostream>

namespace odr::internal::util::byte_stream {

template <typename T> void read(std::istream &in, T &out) {
  in.read(reinterpret_cast<char *>(&out), sizeof(T));
}

template <typename T> T read(std::istream &in) {
  T out;
  read(in, out);
  return out;
}

} // namespace odr::internal::util::byte_stream
