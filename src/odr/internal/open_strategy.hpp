#pragma once

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
struct DecodePreference;
class Logger;
} // namespace odr

namespace odr::internal::abstract {
class File;
class DecodedFile;
class DocumentFile;
} // namespace odr::internal::abstract

namespace odr::internal::open_strategy {

std::vector<FileType>
list_file_types(const std::shared_ptr<abstract::File> &file,
                const Logger &logger);

std::unique_ptr<abstract::DecodedFile>
open_file(const std::shared_ptr<abstract::File> &file, const Logger &logger);
std::unique_ptr<abstract::DecodedFile>
open_file(const std::shared_ptr<abstract::File> &file, FileType as,
          const Logger &logger);
std::unique_ptr<abstract::DecodedFile>
open_file(const std::shared_ptr<abstract::File> &file,
          const DecodePreference &preference, const Logger &logger);

std::unique_ptr<abstract::DocumentFile>
open_document_file(const std::shared_ptr<abstract::File> &file,
                   const Logger &logger);

} // namespace odr::internal::open_strategy
