#include <odr/internal/ooxml/ooxml_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/byte_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

#include <pugixml.hpp>

namespace odr::internal::ooxml::crypto {

namespace {

/// [MS-OFFCRYPTO] 2.3.4.11: the password as UTF-16LE bytes.
std::string password_bytes(const std::string_view password) {
  const std::u16string password_u16 =
      util::string::string_to_u16string(password);
  return {reinterpret_cast<const char *>(password_u16.data()),
          2 * password_u16.size()};
}

/// [MS-OFFCRYPTO] 2.3.4.11 and 2.3.4.12: pads with 0x36 or truncates.
std::string fit(std::string bytes, const std::size_t size) {
  bytes.resize(size, '\x36');
  return bytes;
}

std::string block_key_of(const std::uint32_t block) {
  std::string result;
  util::byte_string::put_u32_le(result, block);
  return result;
}

/// The child element whose name without its namespace prefix is `name`.
pugi::xml_node child_by_local_name(const pugi::xml_node parent,
                                   const std::string_view name) {
  for (const pugi::xml_node child : parent.children()) {
    const std::string_view child_name = child.name();
    const std::size_t colon = child_name.find(':');
    if ((colon == std::string_view::npos
             ? child_name
             : child_name.substr(colon + 1)) == name) {
      return child;
    }
  }
  return {};
}

std::uint32_t read_uint(const pugi::xml_node node, const char *name) {
  const pugi::xml_attribute attribute = node.attribute(name);
  if (attribute.empty()) {
    throw std::runtime_error("ooxml agile encryption lacks an attribute");
  }
  return attribute.as_uint();
}

std::string read_base64(const pugi::xml_node node, const char *name) {
  const pugi::xml_attribute attribute = node.attribute(name);
  if (attribute.empty()) {
    throw std::runtime_error("ooxml agile encryption lacks an attribute");
  }
  return internal::crypto::util::base64_decode(attribute.value());
}

/// [MS-OFFCRYPTO] 2.3.4.10 `CT_KeyData` and `CT_PasswordKeyEncryptor`.
ECMA376Agile::CipherParameters
read_cipher_parameters(const pugi::xml_node node) {
  if (std::string_view(node.attribute("cipherAlgorithm").value()) != "AES" ||
      std::string_view(node.attribute("cipherChaining").value()) !=
          "ChainingModeCBC") {
    throw MsUnsupportedCryptoAlgorithm();
  }
  const std::string_view hash_algorithm =
      node.attribute("hashAlgorithm").value();
  ECMA376Agile::CipherParameters result;
  std::uint32_t expected_hash_size = 0;
  if (hash_algorithm == "SHA1") {
    result.hash = internal::crypto::util::sha1;
    expected_hash_size = 20;
  } else if (hash_algorithm == "SHA256") {
    result.hash = internal::crypto::util::sha256;
    expected_hash_size = 32;
  } else if (hash_algorithm == "SHA384") {
    result.hash = internal::crypto::util::sha384;
    expected_hash_size = 48;
  } else if (hash_algorithm == "SHA512") {
    result.hash = internal::crypto::util::sha512;
    expected_hash_size = 64;
  } else {
    throw MsUnsupportedCryptoAlgorithm();
  }

  result.salt = read_base64(node, "saltValue");
  result.block_size = read_uint(node, "blockSize");
  result.key_bits = read_uint(node, "keyBits");
  result.hash_size = read_uint(node, "hashSize");
  if (result.block_size != 16 ||
      (result.key_bits != 128 && result.key_bits != 192 &&
       result.key_bits != 256)) {
    throw MsUnsupportedCryptoAlgorithm();
  }
  if (result.hash_size != expected_hash_size ||
      result.salt.size() != read_uint(node, "saltSize") ||
      result.salt.empty()) {
    throw std::runtime_error("bad ooxml agile encryption parameters");
  }
  return result;
}

/// [MS-OFFCRYPTO] 2.3.4.13.
constexpr std::string_view verifier_hash_input_block_key =
    "\xfe\xa7\xd2\x76\x3b\x4b\x9e\x79";
constexpr std::string_view verifier_hash_value_block_key =
    "\xd7\xaa\x0f\x6d\x30\x61\x34\x4e";
constexpr std::string_view encrypted_key_value_block_key =
    "\x14\x6e\x0b\xe7\xab\xac\xd0\xd6";

/// [MS-OFFCRYPTO] 2.3.4.15.
constexpr std::size_t segment_size = 4096;

/// [MS-OFFCRYPTO] 2.3.4.10 `ST_SpinCount`.
constexpr std::uint32_t max_spin_count = 10000000;

constexpr std::string_view password_key_encryptor_uri =
    "http://schemas.microsoft.com/office/2006/keyEncryptor/password";

} // namespace

ECMA376Standard::ECMA376Standard(const EncryptionHeader &encryption_header,
                                 const EncryptionVerifier &encryption_verifier,
                                 std::string encrypted_verifier_hash)
    : m_encryption_header{encryption_header},
      m_encryption_verifier{encryption_verifier},
      m_encrypted_verifier_hash{std::move(encrypted_verifier_hash)} {
  validate_();
}

ECMA376Standard::ECMA376Standard(const std::string_view encryption_info) {
  util::byte_string::Reader reader(encryption_info);
  reader.seek(sizeof(VersionInfo));

  StandardHeader standard_header{};
  reader.read(standard_header);

  // [MS-OFFCRYPTO] 2.3.4.5: `encryption_header_size` spans the header plus the
  // trailing CSP name, which is not needed to derive the key.
  if (standard_header.encryption_header_size < sizeof(EncryptionHeader)) {
    throw std::runtime_error("bad ooxml crypto header size");
  }
  reader.read(m_encryption_header);
  reader.skip(standard_header.encryption_header_size -
              sizeof(EncryptionHeader));

  reader.read(m_encryption_verifier);

  // [MS-OFFCRYPTO] 2.3.3: an AES verifier hash is 32 bytes; bytes after it
  // carry nothing, so they do not refuse the file.
  m_encrypted_verifier_hash = reader.rest().substr(0, 32);
  validate_();
}

void ECMA376Standard::validate_() const {
  // [MS-OFFCRYPTO] 2.3.4.5 and 2.3.4.9.
  const auto &header = m_encryption_header;
  if ((header.flags & 0x3cU) != 0x24U || header.alg_id < 0x660eU ||
      header.alg_id > 0x6610U || header.alg_id_hash != 0x8004U ||
      header.key_size != 128U + (header.alg_id - 0x660eU) * 64U) {
    throw MsUnsupportedCryptoAlgorithm();
  }
  if (header.size_extra != 0 || m_encryption_verifier.salt_size != 16 ||
      m_encryption_verifier.verifier_hash_size != 20 ||
      m_encrypted_verifier_hash.size() != 32) {
    throw std::runtime_error("bad ooxml crypto verifier");
  }
}

std::string ECMA376Standard::derive_key(const std::string_view password) const {
  // [MS-OFFCRYPTO] 2.3.4.7.
  std::string hash;
  {
    hash = internal::crypto::util::sha1(
        std::string(m_encryption_verifier.salt.data(),
                    m_encryption_verifier.salt_size) +
        password_bytes(password));
    std::string ibytes(4, ' ');
    for (std::uint32_t i = 0; i < iteration_count; ++i) {
      util::byte::to_little_endian(i, ibytes);
      std::string ih = ibytes;
      ih += hash;
      hash = internal::crypto::util::sha1(ih);
    }
    util::byte::to_little_endian(static_cast<std::uint32_t>(0), ibytes);
    hash = internal::crypto::util::sha1(hash + ibytes);
  }

  std::string result;
  {
    const std::uint32_t cb_required_key_length =
        m_encryption_header.key_size / 8;
    constexpr std::uint32_t cb_hash = 20;

    std::string buf1(64, '\x36');
    buf1 = util::byte::xor_bytes(hash, buf1.substr(0, cb_hash)) +
           buf1.substr(cb_hash);
    const auto x1 = internal::crypto::util::sha1(buf1);
    std::string buf2(64, '\x5c');
    buf2 = util::byte::xor_bytes(hash, buf2.substr(0, cb_hash)) +
           buf2.substr(cb_hash);
    const auto x2 = internal::crypto::util::sha1(buf2);
    const auto x3 = x1 + x2;
    result = x3.substr(0, cb_required_key_length);
  }

  return result;
}

bool ECMA376Standard::verify(const std::string_view key) const {
  // [MS-OFFCRYPTO] 2.3.4.9.

  const std::string verifier = internal::crypto::util::decrypt_aes_ecb(
      key, std::string_view(m_encryption_verifier.encrypted_verifier.data(),
                            sizeof(m_encryption_verifier.encrypted_verifier)));
  const std::string hash = internal::crypto::util::sha1(verifier);
  const std::string verifier_hash =
      internal::crypto::util::decrypt_aes_ecb(key, m_encrypted_verifier_hash)
          .substr(0, hash.size());

  return hash == verifier_hash;
}

std::string ECMA376Standard::decrypt(const std::string_view encrypted_package,
                                     const std::string_view key) const {
  util::byte_string::Reader reader(encrypted_package);
  // [MS-OFFCRYPTO] 2.3.4.4: the stream opens with the plaintext size.
  const auto total_size = reader.read<std::uint64_t>();

  if (total_size > reader.remaining()) {
    throw std::runtime_error("truncated ooxml encrypted package");
  }
  std::string result =
      internal::crypto::util::decrypt_aes_ecb(key, reader.rest());
  result.resize(static_cast<std::size_t>(total_size));
  return result;
}

ECMA376Agile::ECMA376Agile(const std::string_view encryption_info) {
  util::byte_string::Reader reader(encryption_info);
  reader.seek(sizeof(VersionInfo));
  if (reader.read<std::uint32_t>() != 0x40) {
    throw std::runtime_error("bad ooxml agile encryption header");
  }

  const pugi::xml_document document = xml::parse(std::string(reader.rest()));
  const pugi::xml_node encryption = child_by_local_name(document, "encryption");
  if (encryption.empty()) {
    throw std::runtime_error("bad ooxml agile encryption descriptor");
  }
  m_key_data =
      read_cipher_parameters(child_by_local_name(encryption, "keyData"));

  pugi::xml_node encrypted_key;
  for (const pugi::xml_node key_encryptor :
       child_by_local_name(encryption, "keyEncryptors").children()) {
    if (key_encryptor.attribute("uri").value() == password_key_encryptor_uri) {
      encrypted_key = child_by_local_name(key_encryptor, "encryptedKey");
      break;
    }
  }
  if (encrypted_key.empty()) {
    // a certificate key encryptor alone needs a private key, not a password
    throw MsUnsupportedCryptoAlgorithm();
  }

  m_password = read_cipher_parameters(encrypted_key);
  m_spin_count = read_uint(encrypted_key, "spinCount");
  if (m_spin_count > max_spin_count) {
    throw std::runtime_error("bad ooxml agile encryption spin count");
  }
  m_encrypted_verifier_hash_input =
      read_base64(encrypted_key, "encryptedVerifierHashInput");
  m_encrypted_verifier_hash_value =
      read_base64(encrypted_key, "encryptedVerifierHashValue");
  m_encrypted_key_value = read_base64(encrypted_key, "encryptedKeyValue");
}

std::string ECMA376Agile::derive_key(const std::string_view password) const {
  // [MS-OFFCRYPTO] 2.3.4.11, up to the block key.
  std::string hash =
      m_password.hash(m_password.salt + password_bytes(password));
  for (std::uint32_t i = 0; i < m_spin_count; ++i) {
    std::string input = block_key_of(i);
    input += hash;
    hash = m_password.hash(input);
  }
  return hash;
}

std::string
ECMA376Agile::decrypt_with_block_key(const std::string_view key,
                                     const std::string_view block_key,
                                     const std::string_view input) const {
  // [MS-OFFCRYPTO] 2.3.4.13: the salt is the IV, without a block key.
  const std::string block_cipher_key =
      fit(m_password.hash(std::string(key) + std::string(block_key)),
          m_password.key_bits / 8);
  return internal::crypto::util::decrypt_aes_cbc(
      block_cipher_key, fit(m_password.salt, m_password.block_size), input);
}

bool ECMA376Agile::verify(const std::string_view key) const {
  // [MS-OFFCRYPTO] 2.3.4.13.
  const std::string verifier =
      decrypt_with_block_key(key, verifier_hash_input_block_key,
                             m_encrypted_verifier_hash_input)
          .substr(0, m_password.salt.size());
  const std::string verifier_hash =
      decrypt_with_block_key(key, verifier_hash_value_block_key,
                             m_encrypted_verifier_hash_value)
          .substr(0, m_password.hash_size);
  return m_password.hash(verifier) == verifier_hash;
}

std::string ECMA376Agile::decrypt(const std::string_view encrypted_package,
                                  const std::string_view key) const {
  const std::string secret_key =
      decrypt_with_block_key(key, encrypted_key_value_block_key,
                             m_encrypted_key_value)
          .substr(0, m_key_data.key_bits / 8);
  if (secret_key.size() != m_key_data.key_bits / 8) {
    throw std::runtime_error("bad ooxml agile encrypted key");
  }

  util::byte_string::Reader reader(encrypted_package);
  // [MS-OFFCRYPTO] 2.3.4.4: the stream opens with the plaintext size.
  const auto total_size = reader.read<std::uint64_t>();
  const std::string_view ciphertext = reader.rest();
  if (total_size > ciphertext.size()) {
    throw std::runtime_error("truncated ooxml encrypted package");
  }

  // [MS-OFFCRYPTO] 2.3.4.15.
  std::string result;
  result.reserve(ciphertext.size());
  for (std::size_t offset = 0; offset < ciphertext.size();
       offset += segment_size) {
    const auto segment = static_cast<std::uint32_t>(offset / segment_size);
    const std::string iv =
        fit(m_key_data.hash(m_key_data.salt + block_key_of(segment)),
            m_key_data.block_size);
    result += internal::crypto::util::decrypt_aes_cbc(
        secret_key, iv, ciphertext.substr(offset, segment_size));
  }
  result.resize(static_cast<std::size_t>(total_size));
  return result;
}

Util::Util(const std::string_view encryption_info) {
  {
    // big endian is not supported
    constexpr std::uint16_t num = 1;
    if (*reinterpret_cast<const std::uint8_t *>(&num) != 1) {
      throw UnsupportedEndian();
    }
  }

  util::byte_string::Reader reader(encryption_info);
  const auto version_info = reader.read<VersionInfo>();
  if ((version_info.major == 2 || version_info.major == 3 ||
       version_info.major == 4) &&
      version_info.minor == 2) {
    impl = std::make_unique<ECMA376Standard>(encryption_info);
  } else if (version_info.major == 4 && version_info.minor == 4) {
    impl = std::make_unique<ECMA376Agile>(encryption_info);
  } else {
    // extensible (3.3/4.3) and unknown variants
    throw MsUnsupportedCryptoAlgorithm();
  }
}

Util::~Util() = default;

std::string Util::derive_key(const std::string_view password) const {
  return impl->derive_key(password);
}

bool Util::verify(const std::string_view key) const {
  return impl->verify(key);
}

std::string Util::decrypt(const std::string_view encrypted_package,
                          const std::string_view key) const {
  return impl->decrypt(encrypted_package, key);
}

} // namespace odr::internal::ooxml::crypto
