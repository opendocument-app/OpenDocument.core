#ifndef ODR_OPENSTRATEGY_H
#define ODR_OPENSTRATEGY_H

#include <vector>
#include <memory>

namespace odr {
enum class FileType;
namespace common {
class File;
class Document;
}
namespace access {
class Path;
}

namespace OpenStrategy {
std::vector<FileType> types(const access::Path &path);

std::unique_ptr<common::File> openFile(const access::Path &path);
std::unique_ptr<common::File> openFile(const access::Path &path,
                                       FileType as);

std::unique_ptr<common::Document> openDocument(const access::Path &path);
std::unique_ptr<common::Document> openDocument(const access::Path &path,
                                               FileType as);
}

}

#endif // ODR_OPENSTRATEGY_H
