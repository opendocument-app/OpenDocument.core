#pragma once

#include <iosfwd>
#include <memory>
#include <string>

namespace odr::internal::crypto::util {

std::string base64_encode(const std::string &);
std::string base64_decode(const std::string &);

std::string sha1(const std::string &);
std::string sha256(const std::string &);

std::string pbkdf2(std::size_t key_size, const std::string &start_key,
                   const std::string &salt, std::size_t iteration_count);

std::string decrypt_AES(const std::string &key, const std::string &input);
std::string decrypt_AES(const std::string &key, const std::string &iv,
                        const std::string &input);
std::string decrypt_TripleDES(const std::string &key, const std::string &iv,
                              const std::string &input);
std::string decrypt_Blowfish(const std::string &key, const std::string &iv,
                             const std::string &input);

std::string inflate(const std::string &input);
std::size_t padding(const std::string &input);

std::string zlib_inflate(const std::string &input);

} // namespace odr::internal::crypto::util
