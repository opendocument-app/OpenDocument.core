#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace odr::internal::crypto::util {

std::string base64_encode(const std::string &);
std::string base64_decode(const std::string &);

std::string hex_encode(const std::string &);
std::string hex_decode(const std::string &);

/// CRC-32 (ISO 3309 / PNG Annex D, polynomial 0xEDB88320).
std::uint32_t crc32(std::string_view input);
std::string md5(const std::string &);
std::string sha1(const std::string &);
std::string sha256(const std::string &);
std::string sha384(const std::string &);
std::string sha512(const std::string &);

/// RC4 stream cipher; symmetric, so the same call encrypts and decrypts.
std::string rc4(const std::string &key, const std::string &input);

std::string pbkdf2(std::size_t key_size, const std::string &start_key,
                   const std::string &salt, std::size_t iteration_count);
std::string argon2id(std::size_t key_size, const std::string &start_key,
                     const std::string &salt, std::size_t iteration_count,
                     std::size_t memory, std::size_t lanes);

std::string decrypt_aes_ecb(const std::string &key, const std::string &input);
std::string decrypt_aes_cbc(const std::string &key, const std::string &iv,
                            const std::string &input);
/// Raw AES-CBC encryption, no padding (`input` must be a multiple of the block
/// size). Needed by the PDF R 6 hardened-hash algorithm (ISO 32000-2 2.B).
std::string encrypt_aes_cbc(const std::string &key, const std::string &iv,
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
std::string zlib_deflate(const std::string &input);

} // namespace odr::internal::crypto::util
