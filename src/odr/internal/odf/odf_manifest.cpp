#include <odr/internal/odf/odf_manifest.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/map_util.hpp>

#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {
bool lookup_checksum_type(const std::string &checksum,
                          ChecksumType &checksum_type) {
  static const std::unordered_map<std::string, ChecksumType> CHECKSUM_TYPES = {
      {"SHA1", ChecksumType::SHA1},
      {"SHA1/1K", ChecksumType::SHA1_1K},
      {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k",
       ChecksumType::SHA256_1K},
  };
  return util::map::lookup_default(CHECKSUM_TYPES, checksum, checksum_type,
                                   ChecksumType::UNKNOWN);
}

bool lookup_algorithm_types(const std::string &algorithm,
                            AlgorithmType &algorithm_type) {
  static const std::unordered_map<std::string, AlgorithmType> ALGORITHM_TYPES =
      {
          {"http://www.w3.org/2001/04/xmlenc#aes256-cbc",
           AlgorithmType::AES256_CBC},
          {"http://www.w3.org/2001/04/xmlenc#tripledes-cbc",
           AlgorithmType::TRIPLE_DES_CBC},
          {"Blowfish CFB", AlgorithmType::BLOWFISH_CFB},
          {"http://www.w3.org/2009/xmlenc11#aes256-gcm",
           AlgorithmType::AES256_GCM},
      };
  return util::map::lookup_default(ALGORITHM_TYPES, algorithm, algorithm_type,
                                   AlgorithmType::UNKNOWN);
}

bool lookup_key_derivation_types(const std::string &key_derivation,
                                 KeyDerivationType &key_derivation_type) {
  static const std::unordered_map<std::string, KeyDerivationType>
      KEY_DERIVATION_TYPES = {
          {"PBKDF2", KeyDerivationType::PBKDF2},
          {"urn:org:documentfoundation:names:experimental:office:manifest:"
           "argon2id",
           KeyDerivationType::ARGON2ID},
      };
  return util::map::lookup_default(KEY_DERIVATION_TYPES, key_derivation,
                                   key_derivation_type,
                                   KeyDerivationType::UNKNOWN);
}

bool lookup_start_key_types(const std::string &checksum,
                            ChecksumType &checksumType) {
  static const std::unordered_map<std::string, ChecksumType> STARTKEY_TYPES = {
      {"SHA1", ChecksumType::SHA1},
      {"http://www.w3.org/2000/09/xmldsig#sha256", ChecksumType::SHA256},
      {"http://www.w3.org/2001/04/xmlenc#sha256", ChecksumType::SHA256},
  };
  return util::map::lookup_default(STARTKEY_TYPES, checksum, checksumType,
                                   ChecksumType::UNKNOWN);
}
} // namespace

const Manifest::Entry &Manifest::smallest_file_entry() const {
  return entries.at(smallest_file_path);
}

Manifest parse_manifest(const pugi::xml_document &manifest) {
  Manifest result;

  std::optional<std::uint64_t> smallest_file_size;

  for (auto &&e : manifest.child("manifest:manifest").children()) {
    const common::Path path =
        common::Path(e.attribute("manifest:full-path").as_string());
    const pugi::xml_node crypto = e.child("manifest:encryption-data");
    if (!crypto) {
      continue;
    }
    result.encrypted = true;

    Manifest::Entry entry;
    entry.size = e.attribute("manifest:size").as_uint();

    // checksum
    if (crypto.attribute("manifest:checksum-type") &&
        crypto.attribute("manifest:checksum")) {
      entry.checksum.emplace();
      const std::string checksum_type =
          crypto.attribute("manifest:checksum-type").as_string();
      lookup_checksum_type(checksum_type, entry.checksum->type);
      entry.checksum->value = crypto::util::base64_decode(
          crypto.attribute("manifest:checksum").as_string());
    }

    { // encryption algorithm
      const pugi::xml_node algorithm = crypto.child("manifest:algorithm");
      const std::string algorithm_name =
          algorithm.attribute("manifest:algorithm-name").as_string();
      lookup_algorithm_types(algorithm_name, entry.algorithm.type);
      entry.algorithm.initialisation_vector = crypto::util::base64_decode(
          algorithm.attribute("manifest:initialisation-vector").as_string());
    }

    { // key derivation
      const pugi::xml_node key = crypto.child("manifest:key-derivation");
      const std::string key_derivation_name =
          key.attribute("manifest:key-derivation-name").as_string();
      lookup_key_derivation_types(key_derivation_name,
                                  entry.key_derivation.type);
      entry.key_derivation.size =
          key.attribute("manifest:key-size").as_uint(16);
      entry.key_derivation.iteration_count =
          key.attribute("manifest:iteration-count").as_uint();
      entry.key_derivation.salt = crypto::util::base64_decode(
          key.attribute("manifest:salt").as_string());

      if (entry.key_derivation.type == KeyDerivationType::ARGON2ID) {
        entry.key_derivation.iteration_count =
            key.attribute("loext:argon2-iterations").as_uint();
        entry.key_derivation.argon2_memory =
            key.attribute("loext:argon2-memory").as_uint();
        entry.key_derivation.argon2_lanes =
            key.attribute("loext:argon2-lanes").as_uint();
      }
    }

    { // start key generation
      const pugi::xml_node start =
          crypto.child("manifest:start-key-generation");
      if (start) {
        const std::string start_key_generation_name =
            start.attribute("manifest:start-key-generation-name").as_string();
        lookup_start_key_types(start_key_generation_name,
                               entry.start_key_generation.type);
        entry.start_key_generation.size =
            start.attribute("manifest:key-size").as_uint();
      } else {
        entry.start_key_generation.type = ChecksumType::SHA1;
        entry.start_key_generation.size = 20;
      }
    }

    if (!smallest_file_size || (entry.size < *smallest_file_size)) {
      smallest_file_size = entry.size;
      result.smallest_file_path = path;
    }

    result.entries.emplace(path, std::move(entry));
  }

  return result;
}

} // namespace odr::internal::odf
