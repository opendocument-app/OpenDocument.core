#ifndef ODR_OPEN_STRATEGY_H
#define ODR_OPEN_STRATEGY_H

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
}

namespace odr::internal::abstract {
class File;
class DecodedFile;
class DocumentFile;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::open_strategy {
std::vector<FileType> types(std::shared_ptr<internal::abstract::File> file);

std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file);
std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file, FileType as);

std::unique_ptr<internal::abstract::DocumentFile>
open_document_file(std::shared_ptr<internal::abstract::File> file);
} // namespace odr::open_strategy

#endif // ODR_OPEN_STRATEGY_H
