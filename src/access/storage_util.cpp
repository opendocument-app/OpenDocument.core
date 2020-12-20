#include <access/storage.h>
#include <access/storage_util.h>
#include <access/stream_util.h>

namespace odr::access {

std::string StorageUtil::read(const ReadStorage &storage, const Path &path) {
  std::string result;
  auto in = storage.read(path);
  return StreamUtil::read(*in);
}

} // namespace odr::access
