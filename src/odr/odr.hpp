#pragma once

#include <functional>
#include <string>
#include <vector>

namespace odr {
enum class FileType;
enum class FileCategory;
enum class DecoderEngine;
struct DecodePreference;
class DecodedFile;

/// @brief Get the version of the Open Document Reader library.
/// @return The version of the Open Document Reader library.
[[nodiscard]] std::string version() noexcept;
/// @brief Get the commit hash of the Open Document Reader library.
/// @return The commit hash of the Open Document Reader library.
[[nodiscard]] std::string commit() noexcept;

/// @brief Get the file type by the file extension.
/// @param extension The file extension.
/// @return The file type.
[[nodiscard]] FileType type_by_extension(const std::string &extension) noexcept;
/// @brief Get the file category by the file type.
/// @param type The file type.
/// @return The file category.
[[nodiscard]] FileCategory category_by_type(FileType type) noexcept;
/// @brief Get the file type as a string.
/// @param type The file type.
/// @return The file type as a string.
[[nodiscard]] std::string type_to_string(FileType type) noexcept;

/// @brief Get the decoder engine as a string.
/// @param engine The decoder engine.
/// @return The decoder engine as a string.
[[nodiscard]] std::string engine_to_string(DecoderEngine engine);
/// @brief Get the decoder engine by the name.
/// @param engine The name of the decoder engine.
/// @return The decoder engine.
[[nodiscard]] DecoderEngine engine_by_name(const std::string &engine);

/// @brief Get the file types by the file path.
/// @param path The file path.
/// @return The file types.
[[nodiscard]] std::vector<FileType> types(const std::string &path);
/// @brief Get the decoder engines for a file path and file type.
/// @param path The file path.
/// @param as The file type.
/// @return The decoder engines.
[[nodiscard]] std::vector<DecoderEngine> engines(const std::string &path,
                                                 FileType as);

/// @brief Open a file.
/// @param path The file path.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path);
/// @brief Open a file.
/// @param path The file path.
/// @param as The file type.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path, FileType as);
/// @brief Open a file.
/// @param path The file path.
/// @param preference The decode preference.
/// @return The decoded file.
[[nodiscard]] DecodedFile open(const std::string &path,
                               const DecodePreference &preference);

} // namespace odr
