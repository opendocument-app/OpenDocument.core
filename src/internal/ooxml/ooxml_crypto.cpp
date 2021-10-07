#include <cstdint>
#include <cstring>
#include <internal/crypto/crypto_util.h>
#include <internal/ooxml/ooxml_crypto.h>
#include <internal/util/string_util.h>
#include <odr/exceptions.h>
#include <stdexcept>
#include <utility>

namespace {
template <typename I, typename O> void to_little_endian(I in, O &out) {
  for (std::size_t i = 0; i < sizeof(in); ++i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

template <typename I, typename O> void to_big_endian(I in, O &out) {
  for (std::size_t i = sizeof(in) - 1; i >= 0; --i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

std::string xor_bytes(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    throw std::invalid_argument("a.size() != b.size()");

  std::string result(a.size(), ' ');

  for (std::size_t i = 0; i < result.size(); ++i) {
    result[i] = a[i] ^ b[i];
  }

  return result;
}
} // namespace

namespace odr::internal::ooxml::crypto {

ECMA376Standard::ECMA376Standard(const EncryptionHeader &encryption_header,
                                 const EncryptionVerifier &encryption_verifier,
                                 std::string encrypted_verifier_hash)
    : m_encryption_header{encryption_header},
      m_encryption_verifier{encryption_verifier},
      m_encrypted_verifier_hash{std::move(encrypted_verifier_hash)} {}

ECMA376Standard::ECMA376Standard(const std::string &encryption_info) {
  const char *offset = encryption_info.data() + sizeof(VersionInfo);

  StandardHeader standard_header;
  std::memcpy(&standard_header, offset, sizeof(standard_header));
  offset += sizeof(standard_header);

  std::memcpy(&m_encryption_header, offset, sizeof(m_encryption_header));
  std::u16string csp_name_u16(
      reinterpret_cast<const char16_t *>(offset + sizeof(m_encryption_header)));
  const std::string csp_name = util::string::u16string_to_string(csp_name_u16);
  offset += standard_header.encryption_header_size;

  std::memcpy(&m_encryption_verifier, offset, sizeof(m_encryption_verifier));
  offset += sizeof(m_encryption_verifier);

  m_encrypted_verifier_hash = std::string(
      offset, encryption_info.size() - (offset - encryption_info.data()));
}

std::string
ECMA376Standard::derive_key(const std::string &password) const noexcept {
  // https://msdn.microsoft.com/en-us/library/dd925430(v=office.12).aspx

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
      to_little_endian(i, ibytes);
      hash = internal::crypto::util::sha1(ibytes + hash);
    }
    to_little_endian((std::uint32_t)0, ibytes);
    hash = internal::crypto::util::sha1(hash + ibytes);
  }

  std::string result;
  {
    const std::uint32_t cb_required_key_length =
        m_encryption_header.key_size / 8;
    const std::uint32_t cb_hash = 20;

    std::string buf1(64, '\x36');
    buf1 = xor_bytes(hash, buf1.substr(0, cb_hash)) + buf1.substr(cb_hash);
    const auto x1 = internal::crypto::util::sha1(buf1);
    std::string buf2(64, '\x5c');
    buf2 = xor_bytes(hash, buf2.substr(0, cb_hash)) + buf2.substr(cb_hash);
    const auto x2 = internal::crypto::util::sha1(buf2);
    const auto x3 = x1 + x2;
    result = x3.substr(0, cb_required_key_length);
  }

  return result;
}

bool ECMA376Standard::verify(const std::string &key) const noexcept {
  // https://msdn.microsoft.com/en-us/library/dd926426(v=office.12).aspx

  const std::string verifier = internal::crypto::util::decrypt_AES(
      key, std::string(m_encryption_verifier.encrypted_verifier,
                       sizeof(m_encryption_verifier.encrypted_verifier)));
  const std::string hash = internal::crypto::util::sha1(verifier);
  const std::string verifier_hash =
      internal::crypto::util::decrypt_AES(key, m_encrypted_verifier_hash)
          .substr(0, hash.size());

  return hash == verifier_hash;
}

std::string ECMA376Standard::decrypt(const std::string &encrypted_package,
                                     const std::string &key) const noexcept {
  const std::uint64_t total_size =
      *(reinterpret_cast<const std::uint64_t *>(encrypted_package.data()));
  std::string result =
      internal::crypto::util::decrypt_AES(key, encrypted_package.substr(8))
          .substr(0, total_size);

  return result;
}

Util::Util(const std::string &encryption_info) {
  {
    // big endian is not supported
    const std::uint16_t num = 1;
    if (*(reinterpret_cast<const std::uint8_t *>(&num)) != 1) {
      throw UnsupportedEndian();
    }
  }

  VersionInfo version_info;
  std::memcpy(&version_info, encryption_info.data(), sizeof(version_info));
  if (((version_info.major == 2) || (version_info.major == 3) ||
       (version_info.major == 4)) &&
      (version_info.minor == 2)) {
    impl = std::make_unique<ECMA376Standard>(encryption_info);
  } else if ((version_info.major == 4) && (version_info.minor == 4)) {
    throw MsUnsupportedCryptoAlgorithm(); // agile
  } else if (((version_info.major == 3) || (version_info.major == 4)) &&
             (version_info.minor == 3)) {
    throw MsUnsupportedCryptoAlgorithm(); // extensible
  } else {
    throw MsUnsupportedCryptoAlgorithm(); // unknown
  }
}

Util::~Util() noexcept = default;

std::string Util::derive_key(const std::string &password) const noexcept {
  return impl->derive_key(password);
}

bool Util::verify(const std::string &key) const noexcept {
  return impl->verify(key);
}

std::string Util::decrypt(const std::string &encrypted_package,
                          const std::string &key) const noexcept {
  return impl->decrypt(encrypted_package, key);
}

} // namespace odr::internal::ooxml::crypto
