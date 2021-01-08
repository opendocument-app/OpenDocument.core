#ifndef ODR_OPEN_STRATEGY_H
#define ODR_OPEN_STRATEGY_H

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
}

namespace odr::abstract {
class File;
class DocumentFile;
} // namespace odr::abstract

namespace odr::common {
class Path;
} // namespace odr::common

namespace odr::open_strategy {
std::vector<FileType> types(const common::Path &path);

std::unique_ptr<abstract::File> open_file(const common::Path &path);
std::unique_ptr<abstract::File> open_file(const common::Path &path,
                                          FileType as);

std::unique_ptr<abstract::DocumentFile>
open_document_file(const common::Path &path);
} // namespace odr::open_strategy

#endif // ODR_OPEN_STRATEGY_H
