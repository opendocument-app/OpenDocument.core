#include <access/Storage.h>
#include <access/StorageUtil.h>
#include <access/StreamUtil.h>

namespace odr {
namespace access {

std::string StorageUtil::read(const ReadStorage &storage, const Path &path) {
  std::string result;
  auto in = storage.read(path);
  return StreamUtil::read(*in);
}

} // namespace access
} // namespace odr
