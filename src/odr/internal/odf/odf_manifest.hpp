#pragma once

#include <odr/internal/common/path.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
}

namespace odr::internal::odf {

enum class ChecksumType { UNKNOWN, SHA256, SHA1, SHA256_1K, SHA1_1K };
enum class AlgorithmType {
  UNKNOWN,
  AES256_CBC,
  TRIPLE_DES_CBC,
  BLOWFISH_CFB,
  AES256_GCM
};
enum class KeyDerivationType { UNKNOWN, PBKDF2, ARGON2ID };

struct Manifest {
  struct Entry {
    struct Checksum {
      ChecksumType type{ChecksumType::UNKNOWN};
      std::string value;
    };
    struct Algorithm {
      AlgorithmType type{AlgorithmType::UNKNOWN};
      std::string initialisation_vector;
    };
    struct KeyDerivation {
      KeyDerivationType type{KeyDerivationType::UNKNOWN};
      std::uint64_t size{0};
      std::uint64_t iteration_count{0};
      std::string salt;

      // argon2 specific
      std::uint64_t argon2_memory;
      std::uint64_t argon2_lanes;
    };
    struct StartKeyGeneration {
      ChecksumType type{ChecksumType::UNKNOWN};
      std::uint64_t size{0};
    };

    std::size_t size{0};

    std::optional<Checksum> checksum;
    Algorithm algorithm;
    KeyDerivation key_derivation;
    StartKeyGeneration start_key_generation;
  };

  bool encrypted{false};
  std::unordered_map<AbsPath, Entry> entries;

  AbsPath smallest_file_path;

  [[nodiscard]] const Entry &smallest_file_entry() const;
};

Manifest parse_manifest(const pugi::xml_document &manifest);

} // namespace odr::internal::odf
