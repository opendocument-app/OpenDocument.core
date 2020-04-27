#include <access/Storage.h>
#include <access/StorageUtil.h>

namespace odr {
namespace access {

namespace {
constexpr std::uint32_t bufferSize_ = 4096;
}

std::string StorageUtil::read(const ReadStorage &storage, const Path &path) {
  std::string result;
  char buffer[bufferSize_];
  auto in = storage.read(path);

  result.reserve(storage.size(path));
  while (true) {
    const std::uint32_t read = in->read(buffer, bufferSize_);
    if (read == 0)
      break;
    result.append(buffer, read);
  }

  return result;
}

} // namespace access
} // namespace odr
