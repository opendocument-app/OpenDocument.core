#include <access/Storage.h>
#include <access/StorageUtil.h>
#include <access/StreamUtil.h>

namespace odr::access {

std::string StorageUtil::read(const ReadStorage &storage, const Path &path) {
  std::string result;
  auto in = storage.read(path);
  return StreamUtil::read(*in);
}

} // namespace odr::access
