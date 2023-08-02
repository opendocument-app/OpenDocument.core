#ifndef ODR_INTERNAL_ODF_MANIFEST_H
#define ODR_INTERNAL_ODF_MANIFEST_H

#include <cstdint>
#include <odr/internal/common/path.h>
#include <optional>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
}

namespace odr::internal::odf {

enum class ChecksumType { UNKNOWN, SHA256, SHA1, SHA256_1K, SHA1_1K };
enum class AlgorithmType { UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB };
enum class KeyDerivationType { UNKNOWN, PBKDF2 };

struct Manifest {
  struct Entry {
    std::size_t size{0};

    ChecksumType checksum_type{ChecksumType::UNKNOWN};
    std::string checksum;
    AlgorithmType algorithm{AlgorithmType::UNKNOWN};
    std::string initialisation_vector;
    KeyDerivationType key_derivation{KeyDerivationType::UNKNOWN};
    std::uint64_t key_size{0};
    std::uint64_t key_iteration_count{0};
    std::string key_salt;
    ChecksumType start_key_generation{ChecksumType::UNKNOWN};
    std::uint64_t start_key_size{0};
  };

  bool encrypted{false};
  std::unordered_map<common::Path, Entry> entries;

  common::Path smallest_file_path;

  const Entry &smallest_file_entry() const;
};

Manifest parse_manifest(const pugi::xml_document &manifest);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_MANIFEST_H
