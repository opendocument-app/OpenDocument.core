#ifndef ODR_OOXML_CRYPTO_H
#define ODR_OOXML_CRYPTO_H

#include <memory>
#include <string>

namespace odr {
namespace ooxml {

struct UnsupportedEndianException final : public std::exception {
  const char *what() const noexcept final { return "unsupported endian"; }
};

class MsUnsupportedCryptoAlgorithmException final : public std::exception {
public:
  explicit MsUnsupportedCryptoAlgorithmException(std::string name)
      : name(std::move(name)) {}
  const std::string &getName() const { return name; }
  const char *what() const noexcept final {
    return "unsupported crypto algorithm";
  }

private:
  std::string name;
};

namespace Crypto {
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
  std::uint32_t sizeExtra;
  std::uint32_t algId;
  std::uint32_t algIdHash;
  std::uint32_t keySize;
  std::uint32_t providerType;
  std::uint32_t reserved1;
  std::uint32_t reserved2;
  // CSPName variable utf16 string
};

struct EncryptionVerifier {
  // https://msdn.microsoft.com/en-us/library/dd910568(v=office.12).aspx
  // https://github.com/nolze/msoffcrypto-tool/blob/master/msoffcrypto/format/common.py#L35
  std::uint32_t saltSize;
  char salt[16];
  char encryptedVerifier[16];
  std::uint32_t verifierHashSize;
  // EncryptedVerifierHash variable
};

struct StandardHeader {
  std::uint32_t headerFlags;
  std::uint32_t encryptionHeaderSize;
  // EncryptionHeader
  // EncryptionVerifier
};
#pragma pack(pop)

class Algorithm {
public:
  virtual ~Algorithm() noexcept = default;
  virtual std::string deriveKey(const std::string &password) const noexcept = 0;
  virtual bool verify(const std::string &key) const noexcept = 0;
  virtual std::string decrypt(const std::string &encryptedPackage,
                              const std::string &key) const noexcept = 0;
};

class ECMA376Standard final : public Algorithm {
public:
  ECMA376Standard(const EncryptionHeader &, const EncryptionVerifier &,
                  std::string encryptedVerifierHash);
  explicit ECMA376Standard(const std::string &encryptionInfo);

  std::string deriveKey(const std::string &password) const noexcept final;
  bool verify(const std::string &key) const noexcept final;
  std::string decrypt(const std::string &encryptedPackage,
                      const std::string &key) const noexcept final;

private:
  static constexpr auto ITER_COUNT = 50000;

  EncryptionHeader encryptionHeader;
  EncryptionVerifier encryptionVerifier;
  std::string encryptedVerifierHash;
};

class Util final : public Algorithm {
public:
  explicit Util(const std::string &encryptionInfo);
  ~Util() noexcept final;

  std::string deriveKey(const std::string &password) const noexcept final;
  bool verify(const std::string &key) const noexcept final;
  std::string decrypt(const std::string &encryptedPackage,
                      const std::string &key) const noexcept final;

private:
  std::unique_ptr<Algorithm> impl;
};

} // namespace Crypto

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_CRYPTO_H
