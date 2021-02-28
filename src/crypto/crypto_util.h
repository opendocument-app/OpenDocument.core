#ifndef ODR_CRYPTO_UTIL_H
#define ODR_CRYPTO_UTIL_H

#include <string>

namespace odr::crypto::Util {
std::string base64_encode(const std::string &in);
std::string base64_decode(const std::string &in);
std::string sha1(const std::string &);
std::string sha256(const std::string &);
std::string pbkdf2(std::size_t keySize, const std::string &startKey,
                   const std::string &salt, std::size_t iterationCount);
std::string decrypt_AES(const std::string &key, const std::string &input);
std::string decrypt_AES(const std::string &key, const std::string &iv,
                        const std::string &input);
std::string decrypt_TripleDES(const std::string &key, const std::string &iv,
                              const std::string &input);
std::string decrypt_Blowfish(const std::string &key, const std::string &iv,
                             const std::string &input);
std::string inflate(const std::string &input);
std::size_t padding(const std::string &input);
} // namespace odr::crypto::Util

#endif // ODR_CRYPTO_UTIL_H
