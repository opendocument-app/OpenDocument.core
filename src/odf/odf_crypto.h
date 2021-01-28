#ifndef ODR_ODF_CRYPTO_H
#define ODR_ODF_CRYPTO_H

#include <exception>
#include <memory>
#include <odf/odf_manifest.h>
#include <odf/odf_meta.h>
#include <string>

namespace odr::abstract {
class ReadableFilesystem;
} // namespace odr::abstract

namespace odr::odf {
bool can_decrypt(const Manifest::Entry &) noexcept;
std::string hash(const std::string &input, ChecksumType checksumType);
std::string decrypt(const std::string &input, const std::string &derivedKey,
                    const std::string &initialisationVector,
                    AlgorithmType algorithm);
std::string start_key(const Manifest::Entry &, const std::string &password);
std::string derive_key_and_decrypt(const Manifest::Entry &,
                                   const std::string &startKey,
                                   const std::string &input);
bool validate_password(const Manifest::Entry &, std::string decrypted) noexcept;

bool decrypt(std::shared_ptr<abstract::ReadableFilesystem> &, const Manifest &,
             const std::string &password);
} // namespace odr::odf

#endif // ODR_ODF_CRYPTO_H
