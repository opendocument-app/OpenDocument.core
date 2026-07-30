#pragma once

#include <odr/logger.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odr {
enum class FileType;
enum class FileCategory;
struct DecodePreference;
struct FileTypeCapabilities;
class DecodedFile;
enum class DocumentType;

/// @brief Get the version of the library.
/// @return The version of the library.
[[nodiscard]] std::string version();
/// @brief Get the commit hash of the library.
/// @return The commit hash of the library.
[[nodiscard]] std::string commit_hash();
/// @brief Check if the library is dirty (i.e., has uncommitted changes).
/// @return True if the library is dirty, false otherwise.
[[nodiscard]] bool is_dirty() noexcept;
/// @brief Check if the library is built in debug mode.
/// @return True if the library is built in debug mode, false otherwise.
[[nodiscard]] bool is_debug() noexcept;
/// @brief Get the identification string of the library.
/// @return The identification string of the library.
[[nodiscard]] std::string identify() noexcept;

/// @brief Get every file type this library knows about.
/// @return All file types, in declaration order, including
///         @ref FileType::unknown.
[[nodiscard]] std::vector<FileType> all_file_types();

/// @brief Get the file type by the file extension.
/// @param extension The file extension.
/// @return The file type.
[[nodiscard]] FileType
file_type_by_file_extension(const std::string &extension) noexcept;
/// @brief Get every file extension accepted for the file type.
///
/// The first entry is the canonical one. Some file types have no extension of
/// their own (e.g. @ref FileType::office_open_xml_encrypted, which is carried
/// by an ordinary `docx`/`pptx`/`xlsx` file), in which case the result is
/// empty.
/// @param type The file type.
/// @return The file extensions, without a leading dot.
[[nodiscard]] std::span<const std::string_view>
file_extensions_by_file_type(FileType type) noexcept;
/// @brief Get the canonical file extension by the file type.
/// @param type The file type.
/// @return The file extension, without a leading dot.
[[nodiscard]] std::string_view file_extension_by_file_type(FileType type);
/// @brief Get the file category by the file type.
/// @param type The file type.
/// @return The file category.
[[nodiscard]] FileCategory file_category_by_file_type(FileType type) noexcept;
/// @brief Get the document type by the file type.
/// @param type The file type.
/// @return The document type.
[[nodiscard]] DocumentType document_type_by_file_type(FileType type) noexcept;
/// @brief Get the file type as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string file_type_to_string(FileType type);
/// @brief Get the file category as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string file_category_to_string(FileCategory type);
/// @brief Get the document type as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string document_type_to_string(DocumentType type);
/// @brief Get the file type by the MIME type.
/// @param mimetype The MIME type.
/// @return The file type.
[[nodiscard]] FileType
file_type_by_mimetype(std::string_view mimetype) noexcept;
/// @brief Get the canonical MIME type by the file type.
/// @param type The file type.
/// @return The MIME type.
[[nodiscard]] std::string_view mimetype_by_file_type(FileType type);
/// @brief Get every MIME type accepted for the file type.
///
/// The first entry is the canonical one.
/// @param type The file type.
/// @return The MIME types.
[[nodiscard]] std::span<const std::string_view>
mimetypes_by_file_type(FileType type) noexcept;

/// @brief Get what this library can do with the file type.
///
/// Format-level support, i.e. an upper bound — see @ref FileTypeCapabilities.
/// @param type The file type.
/// @return The capabilities.
[[nodiscard]] FileTypeCapabilities
capabilities_by_file_type(FileType type) noexcept;

/// @brief Determine the file types by the file path.
/// @param path The file path.
/// @param logger The logger to use.
/// @return The file types.
[[nodiscard]] std::vector<FileType>
list_file_types(const std::string &path, const Logger &logger = Logger::null());
/// @brief Determine MIME types by the file path.
/// @param path The file path.
/// @param logger The logger to use.
/// @return The MIME types.
[[nodiscard]] std::string_view mimetype(const std::string &path,
                                        const Logger &logger = Logger::null());

/// @brief Open a file.
/// @param path The file path.
/// @param logger The logger to use.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const Logger &logger = Logger::null());
/// @brief Open a file.
/// @param path The file path.
/// @param as The file type.
/// @param logger The logger to use.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path, FileType as,
                               const Logger &logger = Logger::null());
/// @brief Open a file.
/// @param path The file path.
/// @param preference The decode preference.
/// @param logger The logger to use.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const DecodePreference &preference,
                               const Logger &logger = Logger::null());

} // namespace odr
