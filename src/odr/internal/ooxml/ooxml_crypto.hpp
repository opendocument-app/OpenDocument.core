#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace odr::internal::ooxml::crypto {

// TODO support big endian
#pragma pack(push, 1)
struct VersionInfo {
  std::uint16_t major;
  std::uint16_t minor;
};

/// [MS-OFFCRYPTO] 2.3.2.
struct EncryptionHeader {
  std::uint32_t flags;
  std::uint32_t size_extra;
  std::uint32_t alg_id;
  std::uint32_t alg_id_hash;
  std::uint32_t key_size;
  std::uint32_t provider_type;
  std::uint32_t reserved1;
  std::uint32_t reserved2;
  // CSPName variable utf16 string
};

/// [MS-OFFCRYPTO] 2.3.3.
struct EncryptionVerifier {
  std::uint32_t salt_size;
  std::array<char, 16> salt;
  std::array<char, 16> encrypted_verifier;
  std::uint32_t verifier_hash_size;
  // EncryptedVerifierHash variable
};

struct StandardHeader {
  std::uint32_t header_flags;
  std::uint32_t encryption_header_size;
  // EncryptionHeader
  // EncryptionVerifier
};
#pragma pack(pop)

static_assert(sizeof(EncryptionHeader) == 32);
static_assert(sizeof(EncryptionVerifier) == 40);

class Algorithm {
public:
  virtual ~Algorithm() = default;
  [[nodiscard]] virtual std::string
  derive_key(std::string_view password) const = 0;
  [[nodiscard]] virtual bool verify(std::string_view key) const = 0;
  [[nodiscard]] virtual std::string decrypt(std::string_view encrypted_package,
                                            std::string_view key) const = 0;
};

class ECMA376Standard final : public Algorithm {
public:
  ECMA376Standard(const EncryptionHeader &, const EncryptionVerifier &,
                  std::string encrypted_verifier_hash);
  explicit ECMA376Standard(std::string_view encryption_info);

  [[nodiscard]] std::string
  derive_key(std::string_view password) const override;
  [[nodiscard]] bool verify(std::string_view key) const override;
  [[nodiscard]] std::string decrypt(std::string_view encrypted_package,
                                    std::string_view key) const override;

private:
  static constexpr std::uint32_t iteration_count = 50000;

  EncryptionHeader m_encryption_header{};
  EncryptionVerifier m_encryption_verifier{};
  std::string m_encrypted_verifier_hash;

  void validate_() const;
};

/// [MS-OFFCRYPTO] 2.3.4.10. The key is the iterated password hash; the block
/// keys of 2.3.4.13 are derived from it.
class ECMA376Agile final : public Algorithm {
public:
  explicit ECMA376Agile(std::string_view encryption_info);

  [[nodiscard]] std::string
  derive_key(std::string_view password) const override;
  [[nodiscard]] bool verify(std::string_view key) const override;
  [[nodiscard]] std::string decrypt(std::string_view encrypted_package,
                                    std::string_view key) const override;

  using HashFunction = std::string (*)(std::string_view);

  /// The cipher and hash of `keyData` or of the password key encryptor.
  struct CipherParameters {
    std::string salt;
    std::uint32_t block_size{0};
    std::uint32_t key_bits{0};
    std::uint32_t hash_size{0};
    HashFunction hash{nullptr};
  };

private:
  CipherParameters m_key_data;
  CipherParameters m_password;
  std::uint32_t m_spin_count{0};
  std::string m_encrypted_verifier_hash_input;
  std::string m_encrypted_verifier_hash_value;
  std::string m_encrypted_key_value;

  [[nodiscard]] std::string
  decrypt_with_block_key(std::string_view key, std::string_view block_key,
                         std::string_view input) const;
};

class Util final : public Algorithm {
public:
  explicit Util(std::string_view encryption_info);
  ~Util() override;

  [[nodiscard]] std::string
  derive_key(std::string_view password) const override;
  [[nodiscard]] bool verify(std::string_view key) const override;
  [[nodiscard]] std::string decrypt(std::string_view encrypted_package,
                                    std::string_view key) const override;

private:
  std::unique_ptr<Algorithm> impl;
};

} // namespace odr::internal::ooxml::crypto
