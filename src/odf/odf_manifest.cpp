#include <crypto/crypto_util.h>
#include <odf/odf_manifest.h>
#include <pugixml.hpp>
#include <util/map_util.h>

namespace odr::odf {

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

Manifest parseManifest(const pugi::xml_document &manifest) {
  Manifest result;

  for (auto &&e : manifest.child("manifest:manifest").children()) {
    const common::Path path = e.attribute("manifest:full-path").as_string();
    const pugi::xml_node crypto = e.child("manifest:encryption-data");
    if (!crypto)
      continue;
    result.encrypted = true;

    Manifest::Entry entry;
    entry.size = e.attribute("manifest:size").as_uint();

    { // checksum
      const std::string checksumType =
          crypto.attribute("manifest:checksum-type").as_string();
      lookupChecksumType(checksumType, entry.checksumType);
      entry.checksum = crypto.attribute("manifest:checksum").as_string();
    }

    { // encryption algorithm
      const pugi::xml_node algorithm = crypto.child("manifest:algorithm");
      const std::string algorithmName =
          algorithm.attribute("manifest:algorithm-name").as_string();
      lookupAlgorithmTypes(algorithmName, entry.algorithm);
      entry.initialisationVector =
          algorithm.attribute("manifest:initialisation-vector").as_string();
    }

    { // key derivation
      const pugi::xml_node key = crypto.child("manifest:key-derivation");
      const std::string keyDerivationName =
          key.attribute("manifest:key-derivation-name").as_string();
      lookupKeyDerivationTypes(keyDerivationName, entry.keyDerivation);
      entry.keySize = key.attribute("manifest:key-size").as_uint(16);
      entry.keyIterationCount =
          key.attribute("manifest:iteration-count").as_uint();
      entry.keySalt = key.attribute("manifest:salt").as_string();
    }

    { // start key generation
      const pugi::xml_node start =
          crypto.child("manifest:start-key-generation");
      if (start) {
        const std::string startKeyGenerationName =
            start.attribute("manifest:start-key-generation-name").as_string();
        lookupStartKeyTypes(startKeyGenerationName, entry.startKeyGeneration);
        entry.startKeySize = start.attribute("manifest:key-size").as_uint();
      } else {
        entry.startKeyGeneration = ChecksumType::SHA1;
        entry.startKeySize = 20;
      }
    }

    entry.checksum = crypto::Util::base64Decode(entry.checksum);
    entry.initialisationVector =
        crypto::Util::base64Decode(entry.initialisationVector);
    entry.keySalt = crypto::Util::base64Decode(entry.keySalt);

    const auto it = result.entries.emplace(path, entry).first;
    if ((result.smallest_file_path == nullptr) ||
        (entry.size < result.smallestFileSize)) {
      result.smallestFileSize = entry.size;
      result.smallest_file_path = &it->first;
      result.smallest_file_entry = &it->second;
    }
  }

  return result;
}

} // namespace odr::odf
