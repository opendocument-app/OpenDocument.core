#pragma once

#include <odr/file.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {

/// Whether the package is password protected: it has a `.iwpv2` member.
[[nodiscard]] bool
password_encrypted(const abstract::ReadableFilesystem &filesystem);

/// The app of an encrypted package, guessed from its plain member names: a
/// slide makes Keynote, a table store Numbers, anything else Pages. The member
/// that names the app is encrypted.
[[nodiscard]] FileType
guess_encrypted_file_type(const abstract::ReadableFilesystem &filesystem);

/// The package with each encrypted member decrypted when it is opened, and
/// without `.iwpv2` and `.iwph`, or nothing if `password` is wrong. Throws
/// `UnsupportedCryptoAlgorithm` for an unknown `.iwpv2`.
[[nodiscard]] std::optional<std::shared_ptr<abstract::ReadableFilesystem>>
decrypt(std::shared_ptr<abstract::ReadableFilesystem> filesystem,
        std::string_view password);

/// One encrypted member: one or more segments of an IV, AES-128-CBC with a
/// random first block and PKCS#7 padding, and an HMAC-SHA1 keyed with the
/// password. Throws if the padding or the HMAC is wrong.
[[nodiscard]] std::string decrypt_member(std::string_view data,
                                         std::string_view key,
                                         std::string_view password);

} // namespace odr::internal::iwork
