#ifndef ODR_OPENSTRATEGY_H
#define ODR_OPENSTRATEGY_H

#include <memory>

namespace odr {
enum class FileType;
namespace common {
class File;
}
namespace access {
class Path;
}

namespace OpenStrategy {
std::unique_ptr<common::File> openFile(const access::Path &path);
std::unique_ptr<common::File> openFile(const access::Path &path,
                                       FileType as);
}

}

#endif // ODR_OPENSTRATEGY_H
