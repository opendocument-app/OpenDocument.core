#ifndef ODR_OPEN_STRATEGY_H
#define ODR_OPEN_STRATEGY_H

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
namespace common {
class File;
class DocumentFile;
} // namespace common
namespace access {
class Path;
}

namespace OpenStrategy {
std::vector<FileType> types(const access::Path &path);

std::unique_ptr<common::File> openFile(const access::Path &path);
std::unique_ptr<common::File> openFile(const access::Path &path, FileType as);

std::unique_ptr<common::DocumentFile>
openDocumentFile(const access::Path &path);
} // namespace OpenStrategy

} // namespace odr

#endif // ODR_OPEN_STRATEGY_H
