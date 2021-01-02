#ifndef ODR_OPEN_STRATEGY_H
#define ODR_OPEN_STRATEGY_H

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
}

namespace odr::common {
class Path;
class File;
class DocumentFile;
} // namespace odr::common

namespace odr::OpenStrategy {
std::vector<FileType> types(const common::Path &path);

std::unique_ptr<common::File> openFile(const common::Path &path);
std::unique_ptr<common::File> openFile(const common::Path &path, FileType as);

std::unique_ptr<common::DocumentFile>
openDocumentFile(const common::Path &path);
} // namespace odr::OpenStrategy

#endif // ODR_OPEN_STRATEGY_H
