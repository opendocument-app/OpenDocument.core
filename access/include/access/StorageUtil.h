#ifndef ODR_STORAGEUTIL_H
#define ODR_STORAGEUTIL_H

#include <access/Storage.h>
#include <string>

namespace odr {
namespace access {

class Path;
class Storage;

namespace StorageUtil {

extern std::string read(const Storage &, const Path &);

}

} // namespace access
} // namespace odr

#endif // ODR_STORAGEUTIL_H
