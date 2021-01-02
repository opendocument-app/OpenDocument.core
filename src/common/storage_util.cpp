#include <common/storage.h>
#include <common/storage_util.h>
#include <common/stream_util.h>

namespace odr::common {

std::string StorageUtil::read(const ReadStorage &storage, const Path &path) {
  std::string result;
  auto in = storage.read(path);
  return StreamUtil::read(*in);
}

} // namespace odr::common
