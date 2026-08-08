#include <odr/internal/ooxml/ooxml_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/byte_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace odr::internal::ooxml::crypto {

ECMA376Standard::ECMA376Standard(const EncryptionHeader &encryption_header,
                                 const EncryptionVerifier &encryption_verifier,
                                 std::string encrypted_verifier_hash)
    : m_encryption_header{encryption_header},
      m_encryption_verifier{encryption_verifier},
      m_encrypted_verifier_hash{std::move(encrypted_verifier_hash)} {}

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
  const std::size_t encryption_header_begin = reader.position();
  reader.read(m_encryption_header);
  reader.seek(encryption_header_begin + standard_header.encryption_header_size);

  reader.read(m_encryption_verifier);

  m_encrypted_verifier_hash = reader.rest();
}

std::string ECMA376Standard::derive_key(const std::string_view password) const {
  // https://msdn.microsoft.com/en-us/library/dd925430(v=office.12).aspx

  // [MS-OFFCRYPTO] 2.3.3: `salt_size` is fixed at the size of the salt field.
  if (m_encryption_verifier.salt_size != sizeof(m_encryption_verifier.salt)) {
    throw std::runtime_error("bad ooxml crypto salt size");
  }

  std::string hash;
  {
    const std::u16string password_u16 =
        util::string::string_to_u16string(password);
    const std::string password_u16_bytes(
        reinterpret_cast<const char *>(password_u16.data()),
        2 * password_u16.size());

    hash = internal::crypto::util::sha1(
        std::string(m_encryption_verifier.salt,
                    m_encryption_verifier.salt_size) +
        password_u16_bytes);
    std::string ibytes(4, ' ');
    for (std::uint32_t i = 0; i < ITER_COUNT; ++i) {
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
  // https://msdn.microsoft.com/en-us/library/dd926426(v=office.12).aspx

  const std::string verifier = internal::crypto::util::decrypt_aes_ecb(
      key, std::string_view(m_encryption_verifier.encrypted_verifier,
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

  return internal::crypto::util::decrypt_aes_ecb(key, reader.rest())
      .substr(0, total_size);
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
  } else {
    // agile (4.4), extensible (3.3/4.3) and unknown variants
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
