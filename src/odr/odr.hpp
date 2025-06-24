#pragma once

#include <odr/logger.hpp>

#include <functional>
#include <string>
#include <vector>

namespace odr {
enum class FileType;
enum class FileCategory;
enum class DecoderEngine;
struct DecodePreference;
class DecodedFile;
enum class DocumentType;

/// @brief Get the version of the library.
/// @return The version of the library.
[[nodiscard]] std::string version() noexcept;
/// @brief Get the commit hash of the library.
/// @return The commit hash of the library.
[[nodiscard]] std::string commit_hash() noexcept;
/// @brief Check if the library is dirty (i.e., has uncommitted changes).
/// @return True if the library is dirty, false otherwise.
[[nodiscard]] bool is_dirty() noexcept;
/// @brief Check if the library is built in debug mode.
/// @return True if the library is built in debug mode, false otherwise.
[[nodiscard]] bool is_debug() noexcept;
/// @brief Get the identification string of the library.
/// @return The identification string of the library.
[[nodiscard]] std::string identify() noexcept;

/// @brief Get the file type by the file extension.
/// @param extension The file extension.
/// @return The file type.
[[nodiscard]] FileType
file_type_by_file_extension(const std::string &extension) noexcept;
/// @brief Get the file category by the file type.
/// @param type The file type.
/// @return The file category.
[[nodiscard]] FileCategory file_category_by_file_type(FileType type) noexcept;
/// @brief Get the file type as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string file_type_to_string(FileType type) noexcept;
/// @brief Get the file category as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string file_category_to_string(FileCategory type) noexcept;
/// @brief Get the document type as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string document_type_to_string(DocumentType type) noexcept;

/// @brief Get the decoder engine as a string.
/// @param engine The decoder engine.
/// @return The decoder engine as a string.
[[nodiscard]] std::string decoder_engine_to_string(DecoderEngine engine);
/// @brief Get the decoder engine by the name.
/// @param engine The name of the decoder engine.
/// @return The decoder engine.
[[nodiscard]] DecoderEngine decoder_engine_by_name(const std::string &engine);

/// @brief Determine the file types by the file path.
/// @param path The file path.
/// @return The file types.
[[nodiscard]] std::vector<FileType> list_file_types(const std::string &path);
/// @brief Determine the decoder engines for a file path and file type.
/// @param as The file type.
/// @return The decoder engines.
[[nodiscard]] std::vector<DecoderEngine> list_decoder_engines(FileType as);

/// @brief Open a file.
/// @param path The file path.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path,
                               Logger &logger = Logger::null());
/// @brief Open a file.
/// @param path The file path.
/// @param as The file type.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path, FileType as,
                               Logger &logger = Logger::null());
/// @brief Open a file.
/// @param path The file path.
/// @param preference The decode preference.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const DecodePreference &preference,
                               Logger &logger = Logger::null());

} // namespace odr
