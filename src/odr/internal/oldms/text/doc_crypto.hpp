#pragma once

#include <odr/internal/oldms/oldms_crypto.hpp>

#include <optional>
#include <string_view>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms::text {

/// The decrypted WordDocument, Table and Data streams ([MS-DOC] 2.2.6), or
/// nothing if `password` is wrong. `FibBase` loses `fEncrypted`,
/// `fObfuscated` and `lKey`, so the result reads as not encrypted. Throws
/// `MsUnsupportedCryptoAlgorithm` for XOR obfuscation and an unknown
/// encryption.
[[nodiscard]] std::optional<crypto::DecryptedStreams>
decrypt(const abstract::ReadableFilesystem &files, std::string_view password);

} // namespace odr::internal::oldms::text
