#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace odr::internal::pdf {

/// PDF standard security handler (ISO 32000-1 7.6; AES-256 / R 6 per
/// ISO 32000-2 7.6.4). Read-only: once a password authenticates, it decrypts
/// the strings and streams of indirect objects. Permission bits (`/P`) are
/// recorded but not enforced — same stance as the rest of the engine.
///
/// Supported configurations (anything else → `create` returns `nullopt`):
///   - `V 1/2`, `R 2/3` — RC4, 40-128 bit.
///   - `V 4`, `R 4` — crypt filters `StdCF` with `CFM` `V2` (RC4) or `AESV2`
///     (AES-128-CBC), plus `Identity`; honours `StmF`/`StrF`.
///   - `V 5`, `R 6` — AES-256-CBC, file key used directly (no per-object key).
class Decryptor {
public:
  enum class Method {
    none, ///< Identity crypt filter — pass through unchanged.
    rc4,
    aes_v2, ///< AES-128-CBC, per-object key.
    aes_v3, ///< AES-256-CBC, file key used directly (V 5).
  };

  /// Build from the already-reference-resolved `/Encrypt` dictionary and the
  /// first element of the trailer `/ID`. Returns `nullopt` for a non-standard
  /// handler or an unsupported `V`/`R`/`CFM` combination.
  static std::optional<Decryptor> create(const Dictionary &encrypt,
                                         const std::string &file_id0);

  /// Try `password` (UTF-8/PDFDocEncoded bytes) as the user password, then as
  /// the owner password. On success the file key is established and
  /// `authenticated()` becomes true. The empty password is the usual case
  /// (owner-locked-only files).
  bool authenticate(const std::string &password);
  [[nodiscard]] bool authenticated() const;

  [[nodiscard]] std::int64_t permissions() const { return m_p; }

  /// Decrypt the raw bytes of a stream / string belonging to indirect object
  /// `reference`. Streams are decrypted before `/Filter` decoding (7.6.2).
  [[nodiscard]] std::string decrypt_stream(const ObjectReference &reference,
                                           std::string data) const;
  [[nodiscard]] std::string decrypt_string(const ObjectReference &reference,
                                           std::string data) const;

private:
  std::string object_key(const ObjectReference &reference, Method method) const;
  std::string decrypt(const ObjectReference &reference, std::string data,
                      Method method) const;

  std::int64_t m_v{};
  std::int64_t m_r{};
  std::size_t m_key_length{5}; // file key length in bytes
  std::string m_o, m_u, m_oe, m_ue;
  std::int64_t m_p{};
  std::string m_id0;
  bool m_encrypt_metadata{true};
  Method m_stream_method{Method::rc4};
  Method m_string_method{Method::rc4};

  std::optional<std::string> m_file_key;
};

/// Pure password/key algorithms of the standard security handler, exposed for
/// known-answer tests (every one is a `string -> string` function).
namespace standard_security {

/// The 32-byte password padding constant (ISO 32000-1 Algorithm 2, step a).
extern const std::string padding;

/// Algorithm 2: the file encryption key for R 2-4.
std::string compute_key_r2_r4(const std::string &password, const std::string &o,
                              std::int64_t p, const std::string &id0,
                              std::int64_t r, std::size_t key_length,
                              bool encrypt_metadata);

/// Algorithm 4 (R 2) / Algorithm 5 (R 3+): the expected `/U` value for `key`.
/// R 2 yields all 32 bytes; R 3+ yields 16 (callers compare the first 16).
std::string compute_u_r2_r4(const std::string &key, const std::string &id0,
                            std::int64_t r);

/// Algorithm 7: recover the (padded) user password from `/O` using an owner
/// password, for R 2-4.
std::string recover_user_password(const std::string &owner_password,
                                  const std::string &o, std::int64_t r,
                                  std::size_t key_length);

/// ISO 32000-2 Algorithm 2.B: the R 6 hardened hash. `udata` is empty for the
/// user password and the 48-byte `/U` value for the owner password.
std::string hash_r6(const std::string &password, const std::string &salt,
                    const std::string &udata);

} // namespace standard_security

} // namespace odr::internal::pdf
