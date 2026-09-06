#include <odr/odr.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/encoding/text_encoding_table.hpp>
#include <odr/internal/file_type_table.hpp>
#include <odr/internal/git_info.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/project_info.hpp>

#include <algorithm>
#include <iterator>
#include <span>
#include <string_view>

namespace {
using odr::internal::file_type_table::Row;
namespace table = odr::internal::file_type_table;

using EncodingRow = odr::internal::encoding::text_encoding_table::Row;
namespace encoding_table = odr::internal::encoding::text_encoding_table;
} // namespace

std::string odr::version() {
  return std::string(internal::project_info::version());
}

std::string odr::commit_hash() { return internal::git_info::commit_hash(); }

bool odr::is_dirty() noexcept { return internal::git_info::is_dirty(); }

bool odr::is_debug() noexcept { return internal::project_info::is_debug(); }

std::string odr::identify() noexcept {
  return (version().empty() ? "unknown version" : version()) +
         (commit_hash().empty() ? "" : " (" + commit_hash() + ")") +
         (is_dirty() ? " [dirty]" : "") + (is_debug() ? " [debug]" : "");
}

std::vector<odr::FileType> odr::all_file_types() {
  std::vector<FileType> result;
  result.reserve(table::rows().size());
  std::ranges::transform(table::rows(), std::back_inserter(result), &Row::type);
  return result;
}

odr::FileType
odr::file_type_by_file_extension(const std::string &extension) noexcept {
  const Row *row = table::find_by_extension(extension);
  return row == nullptr ? FileType::unknown : row->type;
}

std::span<const std::string_view>
odr::file_extensions_by_file_type(const FileType type) noexcept {
  const Row *row = table::find(type);
  return row == nullptr ? std::span<const std::string_view>{} : row->extensions;
}

std::string_view odr::file_extension_by_file_type(const FileType type) {
  const std::span<const std::string_view> extensions =
      file_extensions_by_file_type(type);
  if (extensions.empty()) {
    throw UnsupportedFileType(type);
  }
  return extensions.front();
}

odr::FileCategory
odr::file_category_by_file_type(const FileType type) noexcept {
  const Row *row = table::find(type);
  return row == nullptr ? FileCategory::unknown : row->category;
}

odr::DocumentType
odr::document_type_by_file_type(const FileType type) noexcept {
  const Row *row = table::find(type);
  return row == nullptr ? DocumentType::unknown : row->document_type;
}

std::string odr::file_type_to_string(const FileType type) {
  const Row *row = table::find(type);
  return row == nullptr ? "unnamed" : std::string(row->name);
}

std::string odr::file_category_to_string(const FileCategory type) {
  switch (type) {
  case FileCategory::unknown:
    return "unknown";
  case FileCategory::archive:
    return "archive";
  case FileCategory::document:
    return "document";
  case FileCategory::image:
    return "image";
  case FileCategory::text:
    return "text";
  case FileCategory::font:
    return "font";
  case FileCategory::audio:
    return "audio";
  case FileCategory::video:
    return "video";
  default:
    return "unnamed";
  }
}

std::string odr::document_type_to_string(const DocumentType type) {
  switch (type) {
  case DocumentType::unknown:
    return "unknown";
  case DocumentType::text:
    return "text";
  case DocumentType::presentation:
    return "presentation";
  case DocumentType::spreadsheet:
    return "spreadsheet";
  case DocumentType::drawing:
    return "drawing";
  default:
    return "unnamed";
  }
}

odr::FileType
odr::file_type_by_mimetype(const std::string_view mimetype) noexcept {
  const Row *row = table::find_by_mimetype(mimetype);
  return row == nullptr ? FileType::unknown : row->type;
}

std::string_view odr::mimetype_by_file_type(const FileType type) {
  const std::span<const std::string_view> mimetypes =
      mimetypes_by_file_type(type);
  if (mimetypes.empty()) {
    throw UnsupportedFileType(type);
  }
  return mimetypes.front();
}

std::span<const std::string_view>
odr::mimetypes_by_file_type(const FileType type) noexcept {
  const Row *row = table::find(type);
  return row == nullptr ? std::span<const std::string_view>{} : row->mimetypes;
}

odr::FileTypeCapabilities
odr::capabilities_by_file_type(const FileType type) noexcept {
  const Row *row = table::find(type);
  return row == nullptr ? FileTypeCapabilities{} : row->capabilities;
}

std::vector<odr::TextEncoding> odr::all_text_encodings() {
  std::vector<TextEncoding> result;
  result.reserve(encoding_table::rows().size());
  std::ranges::transform(encoding_table::rows(), std::back_inserter(result),
                         &EncodingRow::encoding);
  return result;
}

std::string_view odr::text_encoding_to_string(const TextEncoding encoding) {
  const EncodingRow *row = encoding_table::find(encoding);
  if (row == nullptr) {
    throw UnsupportedTextEncoding(encoding);
  }
  return row->names.front();
}

odr::TextEncoding
odr::text_encoding_by_name(const std::string_view name) noexcept {
  const EncodingRow *row = encoding_table::find_by_name(name);
  return row == nullptr ? TextEncoding::unknown : row->encoding;
}

std::span<const std::string_view>
odr::text_encoding_names(const TextEncoding encoding) noexcept {
  const EncodingRow *row = encoding_table::find(encoding);
  return row == nullptr ? std::span<const std::string_view>{} : row->names;
}

bool odr::text_encoding_is_decodable(const TextEncoding encoding) noexcept {
  const EncodingRow *row = encoding_table::find(encoding);
  return row != nullptr && row->decodable;
}

std::vector<odr::FileType> odr::list_file_types(const File &file,
                                                const Logger &logger) {
  return internal::open_strategy::list_file_types(file.impl(), logger);
}

std::vector<odr::FileType> odr::list_file_types(const std::string &path,
                                                const Logger &logger) {
  return list_file_types(File::from_disk(path), logger);
}

std::string_view odr::mimetype(const File &file, const Logger &logger) {
  return internal::magic::mimetype(file.impl(), logger);
}

std::string_view odr::mimetype(const std::string &path, const Logger &logger) {
  return mimetype(File::from_disk(path), logger);
}

odr::DecodedFile odr::open(const File &file, const Logger &logger) {
  return DecodedFile(internal::open_strategy::open_file(file.impl(), logger));
}

odr::DecodedFile odr::open(const File &file, const FileType as,
                           const Logger &logger) {
  return DecodedFile(
      internal::open_strategy::open_file(file.impl(), as, logger));
}

odr::DecodedFile odr::open(const File &file, const DecodePreference &preference,
                           const Logger &logger) {
  return DecodedFile(
      internal::open_strategy::open_file(file.impl(), preference, logger));
}

odr::DecodedFile odr::open(const std::string &path, const Logger &logger) {
  return open(File::from_disk(path), logger);
}

odr::DecodedFile odr::open(const std::string &path, const FileType as,
                           const Logger &logger) {
  return open(File::from_disk(path), as, logger);
}

odr::DecodedFile odr::open(const std::string &path,
                           const DecodePreference &preference,
                           const Logger &logger) {
  return open(File::from_disk(path), preference, logger);
}
