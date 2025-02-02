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
std::string argon2id(std::size_t key_size, const std::string &start_key,
                     const std::string &salt, std::size_t iteration_count,
                     std::size_t memory, std::size_t lanes);

std::string decrypt_aes_ecb(const std::string &key, const std::string &input);
std::string decrypt_aes_cbc(const std::string &key, const std::string &iv,
                            const std::string &input);
std::string decrypt_aes_gcm(const std::string &key, const std::string &iv,
                            const std::string &input);
std::string decrypt_triple_des(const std::string &key, const std::string &iv,
                               const std::string &input);
std::string decrypt_blowfish(const std::string &key, const std::string &iv,
                             const std::string &input);

std::string inflate(const std::string &input);
std::size_t padding(const std::string &input);

std::string zlib_inflate(const std::string &input);

} // namespace odr::internal::crypto::util
