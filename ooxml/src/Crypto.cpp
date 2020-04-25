#include <Crypto.h>
#include <codecvt>
#include <crypto/CryptoUtil.h>
#include <cstdint>
#include <cstring>
#include <locale>

namespace {
template <typename I, typename O> void toLittleEndian(I in, O &out) {
  for (int i = 0; i < sizeof(in); ++i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

template <typename I, typename O> void toBigEndian(I in, O &out) {
  for (int i = sizeof(in) - 1; i >= 0; --i) {
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

namespace odr {
namespace ooxml {

namespace Crypto {

ECMA376Standard::ECMA376Standard(const EncryptionHeader &encryptionHeader,
                                 const EncryptionVerifier &encryptionVerifier,
                                 std::string encryptedVerifierHash)
    : encryptionHeader(encryptionHeader),
      encryptionVerifier(encryptionVerifier),
      encryptedVerifierHash(std::move(encryptedVerifierHash)) {}

ECMA376Standard::ECMA376Standard(const std::string &encryptionInfo) {
  const char *offset = encryptionInfo.data() + sizeof(VersionInfo);

  StandardHeader standardHeader;
  std::memcpy(&standardHeader, offset, sizeof(standardHeader));
  offset += sizeof(standardHeader);

  std::memcpy(&encryptionHeader, offset, sizeof(encryptionHeader));
  std::u16string CSPNameU16((char16_t *)(offset + sizeof(encryptionHeader)));
  const std::string CSPName =
      std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>()
          .to_bytes(CSPNameU16);
  offset += standardHeader.encryptionHeaderSize;

  std::memcpy(&encryptionVerifier, offset, sizeof(encryptionVerifier));
  offset += sizeof(encryptionVerifier);

  encryptedVerifierHash = std::string(
      offset, encryptionInfo.size() - (offset - encryptionInfo.data()));
}

std::string ECMA376Standard::deriveKey(const std::string &password) const
    noexcept {
  // https://msdn.microsoft.com/en-us/library/dd925430(v=office.12).aspx

  std::string hash;
  {
    const std::u16string passwordU16 =
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>()
            .from_bytes(password);
    const std::string passwordU16Bytes((char *)passwordU16.data(),
                                       2 * passwordU16.size());

    hash = crypto::Util::sha1(
        std::string(encryptionVerifier.salt, encryptionVerifier.saltSize) +
        passwordU16Bytes);
    std::string ibytes(4, ' ');
    for (std::uint32_t i = 0; i < ITER_COUNT; ++i) {
      toLittleEndian(i, ibytes);
      hash = crypto::Util::sha1(ibytes + hash);
    }
    toLittleEndian((std::uint32_t)0, ibytes);
    hash = crypto::Util::sha1(hash + ibytes);
  }

  std::string result;
  {
    const std::uint32_t cbRequiredKeyLength = encryptionHeader.keySize / 8;
    const std::uint32_t cbHash = 20;

    std::string buf1(64, '\x36');
    buf1 = xor_bytes(hash, buf1.substr(0, cbHash)) + buf1.substr(cbHash);
    const auto x1 = crypto::Util::sha1(buf1);
    std::string buf2(64, '\x5c');
    buf2 = xor_bytes(hash, buf2.substr(0, cbHash)) + buf2.substr(cbHash);
    const auto x2 = crypto::Util::sha1(buf2);
    const auto x3 = x1 + x2;
    result = x3.substr(0, cbRequiredKeyLength);
  }

  return result;
}

bool ECMA376Standard::verify(const std::string &key) const noexcept {
  // https://msdn.microsoft.com/en-us/library/dd926426(v=office.12).aspx

  const std::string verifier = crypto::Util::decryptAES(
      key, std::string(encryptionVerifier.encryptedVerifier,
                       sizeof(encryptionVerifier.encryptedVerifier)));
  const std::string hash = crypto::Util::sha1(verifier);
  const std::string verifierHash =
      crypto::Util::decryptAES(key, encryptedVerifierHash)
          .substr(0, hash.size());

  return hash == verifierHash;
}

std::string ECMA376Standard::decrypt(const std::string &encryptedPackage,
                                     const std::string &key) const noexcept {
  const std::size_t totalSize = *((const std::size_t *)encryptedPackage.data());
  const std::string result =
      crypto::Util::decryptAES(key, encryptedPackage.substr(8))
          .substr(0, totalSize);

  return result;
}

Util::Util(const std::string &encryptionInfo) {
  {
    // big endian is not supported
    const std::uint16_t num = 1;
    if (*((std::uint8_t *)&num) != 1) {
      throw UnsupportedEndianException();
    }
  }

  VersionInfo versionInfo;
  std::memcpy(&versionInfo, encryptionInfo.data(), sizeof(versionInfo));
  if (((versionInfo.major == 2) || (versionInfo.major == 3) ||
       (versionInfo.major == 4)) &&
      (versionInfo.minor == 2)) {
    impl = std::make_unique<ECMA376Standard>(encryptionInfo);
  } else if ((versionInfo.major == 4) && (versionInfo.minor == 4)) {
    throw MsUnsupportedCryptoAlgorithmException("agile");
  } else if (((versionInfo.major == 3) || (versionInfo.major == 4)) &&
             (versionInfo.minor == 3)) {
    throw MsUnsupportedCryptoAlgorithmException("extensible");
  } else {
    throw MsUnsupportedCryptoAlgorithmException("unknown");
  }
}

Util::~Util() noexcept = default;

std::string Util::deriveKey(const std::string &password) const noexcept {
  return impl->deriveKey(password);
}

bool Util::verify(const std::string &key) const noexcept {
  return impl->verify(key);
}

std::string Util::decrypt(const std::string &encryptedPackage,
                          const std::string &key) const noexcept {
  return impl->decrypt(encryptedPackage, key);
}

} // namespace Crypto

} // namespace ooxml
} // namespace odr
