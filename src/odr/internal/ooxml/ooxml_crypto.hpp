#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace odr::internal::ooxml::crypto {

// TODO support big endian
#pragma pack(push, 1)
struct VersionInfo {
  std::uint16_t major;
  std::uint16_t minor;
};

struct EncryptionHeader {
  // https://msdn.microsoft.com/en-us/library/dd926359(v=office.12).aspx
  // https://github.com/nolze/msoffcrypto-tool/blob/master/msoffcrypto/format/common.py#L9
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

struct EncryptionVerifier {
  // https://msdn.microsoft.com/en-us/library/dd910568(v=office.12).aspx
  // https://github.com/nolze/msoffcrypto-tool/blob/master/msoffcrypto/format/common.py#L35
  std::uint32_t salt_size;
  char salt[16];
  char encrypted_verifier[16];
  std::uint32_t verifierHashSize;
  // EncryptedVerifierHash variable
};

struct StandardHeader {
  std::uint32_t header_flags;
  std::uint32_t encryption_header_size;
  // EncryptionHeader
  // EncryptionVerifier
};
#pragma pack(pop)

class Algorithm {
public:
  virtual ~Algorithm() noexcept = default;
  [[nodiscard]] virtual std::string
  derive_key(const std::string &password) const noexcept = 0;
  [[nodiscard]] virtual bool verify(const std::string &key) const noexcept = 0;
  [[nodiscard]] virtual std::string
  decrypt(const std::string &encrypted_package,
          const std::string &key) const noexcept = 0;
};

class ECMA376Standard final : public Algorithm {
public:
  ECMA376Standard(const EncryptionHeader &, const EncryptionVerifier &,
                  std::string encrypted_verifier_hash);
  explicit ECMA376Standard(const std::string &encryption_info);

  [[nodiscard]] std::string
  derive_key(const std::string &password) const noexcept final;
  [[nodiscard]] bool verify(const std::string &key) const noexcept final;
  [[nodiscard]] std::string
  decrypt(const std::string &encrypted_package,
          const std::string &key) const noexcept final;

private:
  static constexpr auto ITER_COUNT = 50000;

  EncryptionHeader m_encryption_header;
  EncryptionVerifier m_encryption_verifier;
  std::string m_encrypted_verifier_hash;
};

class Util final : public Algorithm {
public:
  explicit Util(const std::string &encryption_info);
  ~Util() noexcept final;

  [[nodiscard]] std::string
  derive_key(const std::string &password) const noexcept final;
  [[nodiscard]] bool verify(const std::string &key) const noexcept final;
  [[nodiscard]] std::string
  decrypt(const std::string &encrypted_package,
          const std::string &key) const noexcept final;

private:
  std::unique_ptr<Algorithm> impl;
};

} // namespace odr::internal::ooxml::crypto
