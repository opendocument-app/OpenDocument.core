#ifndef ODR_STORAGEUTIL_H
#define ODR_STORAGEUTIL_H

#include <string>

namespace odr {

class Path;
class Storage;

namespace StorageUtil {

std::string read(const Storage &, const Path &);

}

}

#endif //ODR_STORAGEUTIL_H
