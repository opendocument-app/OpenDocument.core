#include <internal/crypto/crypto_util.h>
#include <internal/odf/odf_manifest.h>
#include <internal/util/map_util.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {
bool lookupChecksumType(const std::string &checksum,
                        ChecksumType &checksumType) {
  static const std::unordered_map<std::string, ChecksumType> CHECKSUM_TYPES = {
      {"SHA1", ChecksumType::SHA1},
      {"SHA1/1K", ChecksumType::SHA1_1K},
      {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k",
       ChecksumType::SHA256_1K},
  };
  return util::map::lookup_map_default(CHECKSUM_TYPES, checksum, checksumType,
                                       ChecksumType::UNKNOWN);
}

bool lookupAlgorithmTypes(const std::string &algorithm,
                          AlgorithmType &algorithmType) {
  static const std::unordered_map<std::string, AlgorithmType> ALGORITHM_TYPES =
      {
          {"http://www.w3.org/2001/04/xmlenc#aes256-cbc",
           AlgorithmType::AES256_CBC},
          {"", AlgorithmType::TRIPLE_DES_CBC},
          {"Blowfish CFB", AlgorithmType::BLOWFISH_CFB},
      };
  return util::map::lookup_map_default(ALGORITHM_TYPES, algorithm,
                                       algorithmType, AlgorithmType::UNKNOWN);
}

bool lookupKeyDerivationTypes(const std::string &keyDerivation,
                              KeyDerivationType &keyDerivationType) {
  static const std::unordered_map<std::string, KeyDerivationType>
      KEY_DERIVATION_TYPES = {
          {"PBKDF2", KeyDerivationType::PBKDF2},
      };
  return util::map::lookup_map_default(KEY_DERIVATION_TYPES, keyDerivation,
                                       keyDerivationType,
                                       KeyDerivationType::UNKNOWN);
}

bool lookupStartKeyTypes(const std::string &checksum,
                         ChecksumType &checksumType) {
  static const std::unordered_map<std::string, ChecksumType> STARTKEY_TYPES = {
      {"SHA1", ChecksumType::SHA1},
      {"http://www.w3.org/2000/09/xmldsig#sha256", ChecksumType::SHA256},
  };
  return util::map::lookup_map_default(STARTKEY_TYPES, checksum, checksumType,
                                       ChecksumType::UNKNOWN);
}
} // namespace

Manifest parse_manifest(const pugi::xml_document &manifest) {
  Manifest result;

  for (auto &&e : manifest.child("manifest:manifest").children()) {
    const common::Path path = e.attribute("manifest:full-path").as_string();
    const pugi::xml_node crypto = e.child("manifest:encryption-data");
    if (!crypto) {
      continue;
    }
    result.encrypted = true;

    Manifest::Entry entry;
    entry.size = e.attribute("manifest:size").as_uint();

    { // checksum
      const std::string checksumType =
          crypto.attribute("manifest:checksum-type").as_string();
      lookupChecksumType(checksumType, entry.checksum_type);
      entry.checksum = crypto.attribute("manifest:checksum").as_string();
    }

    { // encryption algorithm
      const pugi::xml_node algorithm = crypto.child("manifest:algorithm");
      const std::string algorithmName =
          algorithm.attribute("manifest:algorithm-name").as_string();
      lookupAlgorithmTypes(algorithmName, entry.algorithm);
      entry.initialisation_vector =
          algorithm.attribute("manifest:initialisation-vector").as_string();
    }

    { // key derivation
      const pugi::xml_node key = crypto.child("manifest:key-derivation");
      const std::string keyDerivationName =
          key.attribute("manifest:key-derivation-name").as_string();
      lookupKeyDerivationTypes(keyDerivationName, entry.key_derivation);
      entry.key_size = key.attribute("manifest:key-size").as_uint(16);
      entry.key_iteration_count =
          key.attribute("manifest:iteration-count").as_uint();
      entry.key_salt = key.attribute("manifest:salt").as_string();
    }

    { // start key generation
      const pugi::xml_node start =
          crypto.child("manifest:start-key-generation");
      if (start) {
        const std::string startKeyGenerationName =
            start.attribute("manifest:start-key-generation-name").as_string();
        lookupStartKeyTypes(startKeyGenerationName, entry.start_key_generation);
        entry.start_key_size = start.attribute("manifest:key-size").as_uint();
      } else {
        entry.start_key_generation = ChecksumType::SHA1;
        entry.start_key_size = 20;
      }
    }

    entry.checksum = crypto::util::base64_decode(entry.checksum);
    entry.initialisation_vector =
        crypto::util::base64_decode(entry.initialisation_vector);
    entry.key_salt = crypto::util::base64_decode(entry.key_salt);

    const auto it = result.entries.emplace(path, entry).first;
    if ((result.smallest_file_path == nullptr) ||
        (entry.size < result.smallest_file_size)) {
      result.smallest_file_size = entry.size;
      result.smallest_file_path = &it->first;
      result.smallest_file_entry = &it->second;
    }
  }

  return result;
}

} // namespace odr::internal::odf
