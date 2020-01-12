#ifndef ODR_STORAGEUTIL_H
#define ODR_STORAGEUTIL_H

#include <string>
#include "Storage.h"

namespace odr {

class Path;
class Storage;

namespace StorageUtil {

extern std::string read(const Storage &, const Path &);

extern void deepVisit(const Storage &, Storage::Visitor);
extern void deepVisit(const Storage &, const Path &, Storage::Visitor);

}

}

#endif //ODR_STORAGEUTIL_H
