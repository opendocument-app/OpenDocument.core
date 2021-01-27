#ifndef ODR_OPEN_STRATEGY_H
#define ODR_OPEN_STRATEGY_H

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
}

namespace odr::abstract {
class File;
class DecodedFile;
class DocumentFile;
} // namespace odr::abstract

namespace odr::common {
class Path;
} // namespace odr::common

namespace odr::open_strategy {
std::vector<FileType> types(std::shared_ptr<abstract::File> file);

std::shared_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file);
std::shared_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file, FileType as);

std::unique_ptr<abstract::DocumentFile>
open_document_file(std::shared_ptr<abstract::File> file);
} // namespace odr::open_strategy

#endif // ODR_OPEN_STRATEGY_H
