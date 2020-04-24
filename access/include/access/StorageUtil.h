#ifndef ODR_ACCESS_STORAGE_UTIL_H
#define ODR_ACCESS_STORAGE_UTIL_H

#include <access/Storage.h>
#include <string>

namespace odr {
namespace access {

class Path;
class ReadStorage;

namespace StorageUtil {
extern std::string read(const ReadStorage &, const Path &);
}

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_STORAGE_UTIL_H
