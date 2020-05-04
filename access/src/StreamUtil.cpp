#include <access/StreamUtil.h>

namespace odr {
namespace access {

namespace {
constexpr std::uint32_t bufferSize_ = 4096;
}

std::string StreamUtil::read(std::istream &in) {
  return std::string{std::istreambuf_iterator<char>(in), {}};
}

void StreamUtil::pipe(std::istream &in, std::ostream &out) {
  char buffer[bufferSize_];

  while (true) {
    in.read(buffer, bufferSize_);
    const auto read = in.gcount();
    if (read == 0)
      break;
    out.write(buffer, read);
  }
}

} // namespace access
} // namespace odr
