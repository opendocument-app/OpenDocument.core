#include <cstdint>
#include <internal/util/stream_util.h>
#include <iterator>

namespace odr::internal::util {

std::string stream::read(std::istream &in) {
  return std::string{std::istreambuf_iterator<char>(in), {}};
}

void stream::pipe(std::istream &in, std::ostream &out) {
  static constexpr auto BUFFER_SIZE = 4096;

  char buffer[BUFFER_SIZE];

  while (true) {
    in.read(buffer, BUFFER_SIZE);
    const auto read = in.gcount();
    if (read == 0) {
      break;
    }
    out.write(buffer, read);
  }
}

} // namespace odr::internal::util
