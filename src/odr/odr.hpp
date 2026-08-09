#pragma once

#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odr {

/// @brief The version of the library.
[[nodiscard]] std::string version();
/// @brief The commit the library was built from.
[[nodiscard]] std::string commit_hash();
/// @brief Whether that commit had uncommitted changes.
[[nodiscard]] bool is_dirty() noexcept;
/// @brief Whether the library is built in debug mode.
[[nodiscard]] bool is_debug() noexcept;
/// @brief All of the above in one string.
[[nodiscard]] std::string identify() noexcept;

/// @brief Every file type this library knows about, in declaration order,
/// including @ref FileType::unknown.
[[nodiscard]] std::vector<FileType> all_file_types();

/// @brief The file type for a file extension, @ref FileType::unknown if none.
[[nodiscard]] FileType
file_type_by_file_extension(const std::string &extension) noexcept;
/// @brief Every file extension accepted for the file type, without a leading
/// dot, canonical one first.
///
/// Empty for a file type with no extension of its own, e.g.
/// @ref FileType::office_open_xml_encrypted, which is carried by an ordinary
/// `docx`/`pptx`/`xlsx` file.
[[nodiscard]] std::span<const std::string_view>
file_extensions_by_file_type(FileType type) noexcept;
/// @brief The canonical file extension, without a leading dot.
/// @throws UnsupportedFileType if the type has none.
[[nodiscard]] std::string_view file_extension_by_file_type(FileType type);
/// @brief The file category the file type belongs to.
[[nodiscard]] FileCategory file_category_by_file_type(FileType type) noexcept;
/// @brief The document type the file type carries, if any.
[[nodiscard]] DocumentType document_type_by_file_type(FileType type) noexcept;
/// @brief The file type's name.
[[nodiscard]] std::string file_type_to_string(FileType type);
/// @brief The file category's name.
[[nodiscard]] std::string file_category_to_string(FileCategory type);
/// @brief The document type's name.
[[nodiscard]] std::string document_type_to_string(DocumentType type);
/// @brief The file type for a MIME type, @ref FileType::unknown if none.
[[nodiscard]] FileType
file_type_by_mimetype(std::string_view mimetype) noexcept;
/// @brief The canonical MIME type.
/// @throws UnsupportedFileType if the type has none.
[[nodiscard]] std::string_view mimetype_by_file_type(FileType type);
/// @brief Every MIME type accepted for the file type, canonical one first.
[[nodiscard]] std::span<const std::string_view>
mimetypes_by_file_type(FileType type) noexcept;

/// @brief What this library can do with the file type: format-level support,
/// i.e. an upper bound — see @ref FileTypeCapabilities.
[[nodiscard]] FileTypeCapabilities
capabilities_by_file_type(FileType type) noexcept;

/// @brief Every text encoding this library knows about, in declaration order,
/// excluding @ref TextEncoding::unknown.
[[nodiscard]] std::vector<TextEncoding> all_text_encodings();

/// @brief The text encoding's canonical name, a label a browser accepts.
/// @throws UnsupportedTextEncoding for @ref TextEncoding::unknown.
[[nodiscard]] std::string_view text_encoding_to_string(TextEncoding encoding);
/// @brief The text encoding for a name, @ref TextEncoding::unknown if none.
///
/// Case and any `-`, `_` or space are ignored, so `windows-1252`,
/// `WINDOWS_1252` and `cp1252` all name the same encoding.
[[nodiscard]] TextEncoding
text_encoding_by_name(std::string_view name) noexcept;
/// @brief Every name accepted for the text encoding, canonical one first.
[[nodiscard]] std::span<const std::string_view>
text_encoding_names(TextEncoding encoding) noexcept;
/// @brief Whether the library can decode the text encoding, as opposed to
/// merely naming it.
[[nodiscard]] bool text_encoding_is_decodable(TextEncoding encoding) noexcept;

/// @brief The file types detected for @p file.
[[nodiscard]] std::vector<FileType>
list_file_types(const File &file, const Logger &logger = Logger::null());
/// @brief The file types detected for the file at @p path.
[[nodiscard]] std::vector<FileType>
list_file_types(const std::string &path, const Logger &logger = Logger::null());
/// @brief The MIME type detected for @p file.
[[nodiscard]] std::string_view mimetype(const File &file,
                                        const Logger &logger = Logger::null());
/// @brief The MIME type detected for the file at @p path.
[[nodiscard]] std::string_view mimetype(const std::string &path,
                                        const Logger &logger = Logger::null());

/// @brief Decodes @p file.
[[nodiscard]] DecodedFile open(const File &file,
                               const Logger &logger = Logger::null());
/// @brief Decodes @p file as @p as.
[[nodiscard]] DecodedFile open(const File &file, FileType as,
                               const Logger &logger = Logger::null());
/// @brief Decodes @p file by @p preference.
[[nodiscard]] DecodedFile open(const File &file,
                               const DecodePreference &preference,
                               const Logger &logger = Logger::null());
/// @brief Opens and decodes the file at @p path.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const Logger &logger = Logger::null());
/// @brief Opens the file at @p path, decoding it as @p as.
[[nodiscard]] DecodedFile open(const std::string &path, FileType as,
                               const Logger &logger = Logger::null());
/// @brief Opens the file at @p path, decoding it by @p preference.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const DecodePreference &preference,
                               const Logger &logger = Logger::null());

} // namespace odr
