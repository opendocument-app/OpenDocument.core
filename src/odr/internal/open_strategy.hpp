#pragma once

#include <odr/logger.hpp>

#include <memory>
#include <vector>

namespace odr {
enum class FileType;
enum class DecoderEngine;
struct DecodePreference;
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
list_file_types(const std::shared_ptr<internal::abstract::File> &file,
                Logger &logger = Logger::null());
std::vector<DecoderEngine>
list_decoder_engines(const std::shared_ptr<internal::abstract::File> &file,
                     FileType as);

std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file,
          Logger &logger = Logger::null());
std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file, FileType as,
          Logger &logger = Logger::null());

std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file, FileType as,
          DecoderEngine with, Logger &logger = Logger::null());
std::unique_ptr<internal::abstract::DecodedFile>
open_file(std::shared_ptr<internal::abstract::File> file,
          const DecodePreference &preference, Logger &logger = Logger::null());

std::unique_ptr<internal::abstract::DocumentFile>
open_document_file(std::shared_ptr<internal::abstract::File> file,
                   Logger &logger = Logger::null());
} // namespace odr::internal::open_strategy
