#ifndef ODR_CRYPTOUTIL_H
#define ODR_CRYPTOUTIL_H

#include <string>

namespace odr {

struct OpenDocumentEntry;

namespace CryptoUtil {

std::string base64decode(const std::string &);
std::string sha256(const std::string &);
std::string deriveKey(std::size_t keySize, const std::string &startKey,
                      const std::string &salt, std::size_t iterationCount);
std::string decryptAes(const std::string &key, const std::string &iv, const std::string &input);
std::string deriveKeyAndDecrypt(const OpenDocumentEntry &entry, const std::string &startKey,
                                const std::string &input);
std::string inflate(const std::string &input);

}

}

#endif //ODR_CRYPTOUTIL_H
