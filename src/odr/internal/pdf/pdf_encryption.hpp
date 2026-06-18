#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace odr::internal::pdf {

/// Per-stream / per-string decryption method, named by the crypt filter `/CFM`
/// (V 4/5) or implied by `V` (RC4 for V 1/2).
enum class EncryptionMethod {
  none, ///< Identity crypt filter — pass through unchanged.
  rc4,
  aes_v2, ///< AES-128-CBC, per-object key.
  aes_v3, ///< AES-256-CBC, file key used directly (V 5).
};

/// The decrypting half of the PDF standard security handler (ISO 32000-1 7.6):
/// holds the derived file encryption key and the stream/string methods, and
/// decrypts the strings and streams of indirect objects. Obtained from
/// `Authenticator::authenticate`; immutable and always usable — there is no
/// un-authenticated state. Read-only: permission bits are not enforced.
class Decryptor {
public:
  /// Wrap an already-derived file key and the resolved stream/string methods.
  /// Normally produced by `Authenticator::authenticate` rather than directly.
  Decryptor(std::string key, EncryptionMethod stream_method,
            EncryptionMethod string_method);

  /// Decrypt the raw bytes of a stream / string belonging to indirect object
  /// `reference`. Streams are decrypted before `/Filter` decoding (7.6.2).
  [[nodiscard]] std::string decrypt_stream(const ObjectReference &reference,
                                           const std::string &data) const;
  [[nodiscard]] std::string decrypt_string(const ObjectReference &reference,
                                           const std::string &data) const;

  /// Decrypt every string leaf of `object` in place with the owning object's
  /// reference (ISO 32000-1 7.6.2). Used on freshly read indirect objects.
  void decrypt_strings(Object &object, const ObjectReference &reference);

private:
  [[nodiscard]] std::string object_key(const ObjectReference &reference,
                                       EncryptionMethod method) const;
  [[nodiscard]] std::string decrypt(const ObjectReference &reference,
                                    const std::string &data,
                                    EncryptionMethod method) const;

  std::string m_key;
  EncryptionMethod m_stream_method{EncryptionMethod::rc4};
  EncryptionMethod m_string_method{EncryptionMethod::rc4};
};

/// The authenticating half of the PDF standard security handler (ISO 32000-1
/// 7.6; AES-256 / R 6 per ISO 32000-2 7.6.4): parses the `/Encrypt` dictionary
/// and validates a password, producing a `Decryptor` on success. Permission
/// bits (`/P`) are recorded but not enforced — same stance as the rest of the
/// engine.
///
/// Supported configurations (anything else → `create` returns `nullopt`):
///   - `V 1/2`, `R 2/3` — RC4, 40-128 bit.
///   - `V 4`, `R 4` — crypt filters `StdCF` with `CFM` `V2` (RC4) or `AESV2`
///     (AES-128-CBC), plus `Identity`; honours `StmF`/`StrF`.
///   - `V 5`, `R 6` — AES-256-CBC, file key used directly (no per-object key).
class Authenticator {
public:
  /// Build from the already-reference-resolved `/Encrypt` dictionary and the
  /// first element of the trailer `/ID`. Returns `nullopt` for a non-standard
  /// handler or an unsupported `V`/`R`/`CFM` combination.
  static std::optional<Authenticator> create(const Dictionary &encrypt,
                                             const std::string &file_id0);

  /// The raw `/P` permission bitfield (recorded, not enforced).
  [[nodiscard]] std::int64_t permissions() const { return m_p; }

  /// Try `password` (UTF-8/PDFDocEncoded bytes) as the user password, then as
  /// the owner password. Returns a ready-to-use `Decryptor` on success, or
  /// `nullopt` if neither matches. The empty password is the usual case
  /// (owner-locked-only files). The derived key lives only inside the returned
  /// `Decryptor` — callers carry the `Decryptor` forward (e.g. to re-open the
  /// file for rendering) rather than the bare key, so the password is never
  /// retained.
  [[nodiscard]] std::optional<Decryptor>
  authenticate(const std::string &password) const;

private:
  std::int64_t m_v{};
  std::int64_t m_r{};
  std::size_t m_key_length{5}; ///< file key length in bytes
  std::string m_o;
  std::string m_u;
  std::string m_oe;
  std::string m_ue;
  std::int64_t m_p{};
  std::string m_id0;
  bool m_encrypt_metadata{true};

  EncryptionMethod m_stream_method{EncryptionMethod::rc4};
  EncryptionMethod m_string_method{EncryptionMethod::rc4};
};

/// Pure password/key algorithms of the standard security handler, exposed for
/// known-answer tests (each is a `string -> string` function). The remaining
/// algorithms (Algorithm 7 owner-password recovery, the R 6 hardened hash) are
/// file-local to `pdf_encryption.cpp` — they have no callers outside.
namespace standard_security {

/// Algorithm 2: the file encryption key for R 2-4.
std::string compute_key_r2_r4(const std::string &password, const std::string &o,
                              std::int64_t p, const std::string &id0,
                              std::int64_t r, std::size_t key_length,
                              bool encrypt_metadata);

/// Algorithm 4 (R 2) / Algorithm 5 (R 3+): the expected `/U` value for `key`.
/// R 2 yields all 32 bytes; R 3+ yields 16 (callers compare the first 16).
std::string compute_u_r2_r4(const std::string &key, const std::string &id0,
                            std::int64_t r);

} // namespace standard_security

} // namespace odr::internal::pdf
