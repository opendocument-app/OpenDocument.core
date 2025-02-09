#pragma once

#include <odr/internal/odf/odf_manifest.hpp>
#include <odr/internal/odf/odf_meta.hpp>

#include <exception>
#include <memory>
#include <string>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

bool can_decrypt(const Manifest::Entry &) noexcept;

std::string hash(const std::string &input, ChecksumType checksum_type);

std::string decrypt(const std::string &input, const std::string &derived_key,
                    const std::string &initialisation_vector,
                    AlgorithmType algorithm);

std::string start_key(const Manifest::Entry &, const std::string &password);

std::string derive_key(const Manifest::Entry &, const std::string &start_key);

std::string derive_key_and_decrypt(const Manifest::Entry &,
                                   const std::string &start_key,
                                   const std::string &input);

bool validate_password(const Manifest::Entry &, std::string decrypted) noexcept;

bool decrypt(std::shared_ptr<abstract::ReadableFilesystem> &, const Manifest &,
             const std::string &password);

} // namespace odr::internal::odf
