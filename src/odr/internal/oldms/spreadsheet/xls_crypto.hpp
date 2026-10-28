#pragma once

#include <odr/internal/oldms/oldms_crypto.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms::spreadsheet {

/// The `/Workbook` stream with its record data decrypted ([MS-XLS] 2.2.10),
/// or nothing if `password` is wrong. The FilePass record becomes a zero-filled
/// record of type 0, so the result reads as not encrypted. Throws
/// `MsUnsupportedCryptoAlgorithm` for an unknown encryption.
[[nodiscard]] std::optional<std::string>
decrypt_workbook(std::string workbook, std::string_view password);

/// The decrypted `/Workbook` stream, or nothing if `password` is wrong.
[[nodiscard]] std::optional<crypto::DecryptedStreams>
decrypt(const abstract::ReadableFilesystem &files, std::string_view password);

} // namespace odr::internal::oldms::spreadsheet
