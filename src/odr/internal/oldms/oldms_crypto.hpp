#pragma once

#include <odr/internal/common/path.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::oldms::crypto {

/// The streams that decryption replaces, by path.
using DecryptedStreams = std::vector<std::pair<AbsPath, std::string>>;

/// The keys of RC4 encryption ([MS-OFFCRYPTO] 2.3.6) or of RC4 CryptoAPI
/// encryption (2.3.5) for one password.
class Rc4Keys final {
public:
  enum class Kind { rc4, crypto_api };

  Rc4Keys(Kind kind, std::string password_hash, std::uint32_t key_size);

  /// The RC4 key of a block (2.3.5.2, 2.3.6.2).
  [[nodiscard]] std::string block_key(std::uint32_t block) const;
  /// The keystream of the bytes [0, size) of a stream whose key changes every
  /// `block_size` bytes.
  [[nodiscard]] std::string keystream(std::size_t size,
                                      std::size_t block_size) const;

private:
  Kind m_kind;
  std::string m_password_hash;
  /// In bytes.
  std::uint32_t m_key_size;
};

/// The RC4 encryption header (2.3.6.1) or the RC4 CryptoAPI encryption header
/// (2.3.5.1).
class Rc4Encryption final {
public:
  /// Reads the header at the start of `data`. Throws
  /// `MsUnsupportedCryptoAlgorithm` for another version or algorithm.
  explicit Rc4Encryption(std::string_view data);

  /// The keys for `password`, or nothing if the password is wrong (2.3.5.6,
  /// 2.3.6.4).
  [[nodiscard]] std::optional<Rc4Keys> unlock(std::string_view password) const;

private:
  Rc4Keys::Kind m_kind{Rc4Keys::Kind::rc4};
  std::uint32_t m_key_size{16};
  std::string m_salt;
  std::string m_encrypted_verifier;
  std::string m_encrypted_verifier_hash;
};

using XorArray = std::array<std::uint8_t, 16>;

/// The XOR array of XOR obfuscation method 1 (2.3.7.2) for `password`, or
/// nothing if `password` does not match `key` and `verifier` (2.3.7.1).
std::optional<XorArray> unlock_xor_method1(std::uint16_t key,
                                           std::uint16_t verifier,
                                           std::string_view password);

/// `DecryptData_Method1` (2.3.7.3) on `data`, in place, from `index` into the
/// XOR array.
void decrypt_xor_method1(const XorArray &xor_array, std::size_t index,
                         std::span<char> data);

} // namespace odr::internal::oldms::crypto
