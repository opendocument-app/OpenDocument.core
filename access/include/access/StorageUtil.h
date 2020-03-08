#ifndef ODR_STORAGEUTIL_H
#define ODR_STORAGEUTIL_H

#include <string>
#include <access/Storage.h>

namespace odr {

class Path;
class Storage;

namespace StorageUtil {

extern std::string read(const Storage &, const Path &);

}

}

#endif //ODR_STORAGEUTIL_H
