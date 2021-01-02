#ifndef ODR_COMMON_STORAGE_UTIL_H
#define ODR_COMMON_STORAGE_UTIL_H

#include <common/storage.h>
#include <string>

namespace odr::common {
class Path;
class ReadStorage;

namespace StorageUtil {
std::string read(const ReadStorage &, const Path &);
}

} // namespace odr::common

#endif // ODR_COMMON_STORAGE_UTIL_H
