#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace odr::internal::crypto::util {

std::string base64_encode(std::string_view);
std::string base64_decode(std::string_view);

std::string hex_encode(std::string_view);
std::string hex_decode(std::string_view);

/// CRC-32 (ISO 3309 / PNG Annex D, polynomial 0xEDB88320).
std::uint32_t crc32(std::string_view input);
std::string md5(std::string_view);
std::string sha1(std::string_view);
std::string sha256(std::string_view);
std::string sha384(std::string_view);
std::string sha512(std::string_view);

/// RC4 stream cipher; symmetric, so the same call encrypts and decrypts.
std::string rc4(std::string_view key, std::string_view input);

std::string pbkdf2(std::size_t key_size, std::string_view start_key,
                   std::string_view salt, std::size_t iteration_count);
std::string argon2id(std::size_t key_size, std::string_view start_key,
                     std::string_view salt, std::size_t iteration_count,
                     std::size_t memory, std::size_t lanes);

std::string decrypt_aes_ecb(std::string_view key, std::string_view input);
std::string decrypt_aes_cbc(std::string_view key, std::string_view iv,
                            std::string_view input);
/// Raw AES-CBC encryption, no padding (`input` must be a multiple of the block
/// size). Needed by the PDF R 6 hardened-hash algorithm (ISO 32000-2 2.B).
std::string encrypt_aes_cbc(std::string_view key, std::string_view iv,
                            std::string_view input);
/// AES-GCM per XML Encryption 1.1 §5.2.4: @p input is `iv || ciphertext ||
/// 16-byte tag` and must repeat @p iv. Throws if it does not, if @p input is
/// too short to hold both, or if the tag fails to verify.
std::string decrypt_aes_gcm(std::string_view key, std::string_view iv,
                            std::string_view input);
std::string decrypt_triple_des(std::string_view key, std::string_view iv,
                               std::string_view input);
std::string decrypt_blowfish(std::string_view key, std::string_view iv,
                             std::string_view input);

std::string inflate(std::string_view input);
std::size_t padding(std::string_view input);

std::string zlib_inflate(std::string_view input);
std::string zlib_deflate(std::string_view input);

} // namespace odr::internal::crypto::util
