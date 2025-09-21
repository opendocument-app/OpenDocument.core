#pragma once

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
enum class DecoderEngine;
struct DecodePreference;
class Logger;
} // namespace odr

namespace odr::internal::abstract {
class File;
class DecodedFile;
class DocumentFile;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::open_strategy {
std::vector<FileType>
list_file_types(const std::shared_ptr<abstract::File> &file, Logger &logger);
std::vector<DecoderEngine> list_decoder_engines(FileType as);

std::unique_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file, Logger &logger);
std::unique_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file, FileType as, Logger &logger);

std::unique_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file, FileType as, DecoderEngine with,
          Logger &logger);
std::unique_ptr<abstract::DecodedFile>
open_file(std::shared_ptr<abstract::File> file,
          const DecodePreference &preference, Logger &logger);

std::unique_ptr<abstract::DocumentFile>
open_document_file(std::shared_ptr<abstract::File> file, Logger &logger);
} // namespace odr::internal::open_strategy
