#ifndef ODR_OPENDOCUMENTCRYPTO_H
#define ODR_OPENDOCUMENTCRYPTO_H

#include "OpenDocumentMeta.h"
#include <exception>
#include <memory>
#include <string>

namespace odr {
namespace access {
class Storage;
}
} // namespace odr

namespace odr {
namespace odf {

class UnsupportedCryptoAlgorithmException : public std::exception {
public:
  const char *what() const noexcept override {
    return "unsupported crypto algorithm";
  }
};

namespace OpenDocumentCrypto {

extern bool canDecrypt(const OpenDocumentMeta::Manifest::Entry &) noexcept;
extern std::string hash(const std::string &input,
                        OpenDocumentMeta::ChecksumType checksumType);
extern std::string decrypt(const std::string &input,
                           const std::string &derivedKey,
                           const std::string &initialisationVector,
                           OpenDocumentMeta::AlgorithmType algorithm);
extern std::string startKey(const OpenDocumentMeta::Manifest::Entry &,
                            const std::string &password);
extern std::string
deriveKeyAndDecrypt(const OpenDocumentMeta::Manifest::Entry &,
                    const std::string &startKey, const std::string &input);
extern bool validatePassword(const OpenDocumentMeta::Manifest::Entry &,
                             std::string decrypted) noexcept;

extern bool decrypt(std::unique_ptr<access::Storage> &,
                    const OpenDocumentMeta::Manifest &,
                    const std::string &password);

} // namespace OpenDocumentCrypto

} // namespace odf
} // namespace odr

#endif // ODR_OPENDOCUMENTCRYPTO_H
