#ifndef ODR_ODF_CRYPTO_H
#define ODR_ODF_CRYPTO_H

#include <Meta.h>
#include <exception>
#include <memory>
#include <string>

namespace odr::access {
class ReadStorage;
} // namespace odr::access

namespace odr::odf {

struct UnsupportedCryptoAlgorithmException final : public std::exception {
  const char *what() const noexcept final {
    return "unsupported crypto algorithm";
  }
};

namespace Crypto {
bool canDecrypt(const Meta::Manifest::Entry &) noexcept;
std::string hash(const std::string &input, Meta::ChecksumType checksumType);
std::string decrypt(const std::string &input, const std::string &derivedKey,
                    const std::string &initialisationVector,
                    Meta::AlgorithmType algorithm);
std::string startKey(const Meta::Manifest::Entry &,
                     const std::string &password);
std::string deriveKeyAndDecrypt(const Meta::Manifest::Entry &,
                                const std::string &startKey,
                                const std::string &input);
bool validatePassword(const Meta::Manifest::Entry &,
                      std::string decrypted) noexcept;

bool decrypt(std::unique_ptr<access::ReadStorage> &, const Meta::Manifest &,
             const std::string &password);
} // namespace Crypto

} // namespace odr::odf

#endif // ODR_ODF_CRYPTO_H
