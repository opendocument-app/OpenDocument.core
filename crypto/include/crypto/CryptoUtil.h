#ifndef ODR_CRYPTO_UTIL_H
#define ODR_CRYPTO_UTIL_H

#include <string>

namespace odr {
namespace crypto {

namespace Util {
std::string base64Encode(const std::string &);
std::string base64Decode(const std::string &);
std::string sha1(const std::string &);
std::string sha256(const std::string &);
std::string pbkdf2(std::size_t keySize, const std::string &startKey,
                   const std::string &salt, std::size_t iterationCount);
std::string decryptAES(const std::string &key, const std::string &input);
std::string decryptAES(const std::string &key, const std::string &iv,
                       const std::string &input);
std::string decryptTripleDES(const std::string &key, const std::string &iv,
                             const std::string &input);
std::string decryptBlowfish(const std::string &key, const std::string &iv,
                            const std::string &input);
std::string inflate(const std::string &input);
std::size_t padding(const std::string &input);
} // namespace Util

} // namespace crypto
} // namespace odr

#endif // ODR_CRYPTO_UTIL_H
