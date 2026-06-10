#include <odr/internal/util/byte_stream_util.hpp>

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

} // namespace odr::internal::util
