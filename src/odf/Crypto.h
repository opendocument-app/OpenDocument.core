#ifndef ODR_ODF_CRYPTO_H
#define ODR_ODF_CRYPTO_H

#include <exception>
#include <memory>
#include <odf/Manifest.h>
#include <odf/Meta.h>
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
bool canDecrypt(const Manifest::Entry &) noexcept;
std::string hash(const std::string &input, ChecksumType checksumType);
std::string decrypt(const std::string &input, const std::string &derivedKey,
                    const std::string &initialisationVector,
                    AlgorithmType algorithm);
std::string startKey(const Manifest::Entry &, const std::string &password);
std::string deriveKeyAndDecrypt(const Manifest::Entry &,
                                const std::string &startKey,
                                const std::string &input);
bool validatePassword(const Manifest::Entry &, std::string decrypted) noexcept;

bool decrypt(std::shared_ptr<access::ReadStorage> &, const Manifest &,
             const std::string &password);
} // namespace Crypto

} // namespace odr::odf

#endif // ODR_ODF_CRYPTO_H
