#pragma once

#include <odr/internal/oldms/oldms_crypto.hpp>

#include <optional>
#include <string_view>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms::presentation {

/// The decrypted "Current User", "PowerPoint Document" and "Pictures" streams
/// ([MS-PPT] 2.3.7), or nothing if `password` is wrong. The CurrentUserAtom
/// gets the plain header token, so the result reads as not encrypted.
[[nodiscard]] std::optional<crypto::DecryptedStreams>
decrypt(const abstract::ReadableFilesystem &files, std::string_view password);

} // namespace odr::internal::oldms::presentation
