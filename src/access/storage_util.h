#ifndef ODR_ACCESS_STORAGE_UTIL_H
#define ODR_ACCESS_STORAGE_UTIL_H

#include <access/storage.h>
#include <string>

namespace odr::access {

class Path;
class ReadStorage;

namespace StorageUtil {
extern std::string read(const ReadStorage &, const Path &);
}

} // namespace odr::access

#endif // ODR_ACCESS_STORAGE_UTIL_H
