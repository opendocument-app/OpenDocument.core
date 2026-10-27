#include <odr/internal/oldms/oldms_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/string_util.hpp>

#include <stdexcept>

namespace odr::internal::oldms {

namespace {

std::string password_bytes(const std::string_view password) {
  const std::u16string password_u16 =
      util::string::string_to_u16string(password);
  return {reinterpret_cast<const char *>(password_u16.data()),
          2 * password_u16.size()};
}

/// The next `size` bytes; throws if fewer are left.
std::string read_string(util::byte_string::Reader &reader,
                        const std::size_t size) {
  std::string result(reader.rest().substr(0, size));
  reader.skip(size);
  return result;
}

std::string le32(const std::uint32_t value) {
  std::string result;
  util::byte_string::put_u32_le(result, value);
  return result;
}

/// [MS-OFFCRYPTO] 2.3.7.2.
constexpr std::array<std::uint8_t, 15> pad_array = {
    0xbb, 0xff, 0xff, 0xba, 0xff, 0xff, 0xb9, 0x80,
    0x00, 0xbe, 0x0f, 0x00, 0xbf, 0x0f, 0x00};
constexpr std::array<std::uint16_t, 15> initial_code = {
    0xe1f0, 0x1d0f, 0xcc9c, 0x84c0, 0x110c, 0x0e10, 0xf1ce, 0x313e,
    0x1872, 0xe139, 0xd40f, 0x84f9, 0x280c, 0xa96a, 0x4ec3};
constexpr std::array<std::uint16_t, 105> xor_matrix = {
    0xaefc, 0x4dd9, 0x9bb2, 0x2745, 0x4e8a, 0x9d14, 0x2a09, 0x7b61, 0xf6c2,
    0xfda5, 0xeb6b, 0xc6f7, 0x9dcf, 0x2bbf, 0x4563, 0x8ac6, 0x05ad, 0x0b5a,
    0x16b4, 0x2d68, 0x5ad0, 0x0375, 0x06ea, 0x0dd4, 0x1ba8, 0x3750, 0x6ea0,
    0xdd40, 0xd849, 0xa0b3, 0x5147, 0xa28e, 0x553d, 0xaa7a, 0x44d5, 0x6f45,
    0xde8a, 0xad35, 0x4a4b, 0x9496, 0x390d, 0x721a, 0xeb23, 0xc667, 0x9cef,
    0x29ff, 0x53fe, 0xa7fc, 0x5fd9, 0x47d3, 0x8fa6, 0x0f6d, 0x1eda, 0x3db4,
    0x7b68, 0xf6d0, 0xb861, 0x60e3, 0xc1c6, 0x93ad, 0x377b, 0x6ef6, 0xddec,
    0x45a0, 0x8b40, 0x06a1, 0x0d42, 0x1a84, 0x3508, 0x6a10, 0xaa51, 0x4483,
    0x8906, 0x022d, 0x045a, 0x08b4, 0x1168, 0x76b4, 0xed68, 0xcaf1, 0x85c3,
    0x1ba7, 0x374e, 0x6e9c, 0x3730, 0x6e60, 0xdcc0, 0xa9a1, 0x4363, 0x86c6,
    0x1dad, 0x3331, 0x6662, 0xccc4, 0x89a9, 0x0373, 0x06e6, 0x0dcc, 0x1021,
    0x2042, 0x4084, 0x8108, 0x1231, 0x2462, 0x48c4};

/// [MS-XLS] 2.2.10: the password in the ANSI code page, as ECMA-376 Part 4
/// 15.3.3 `revisionsPassword` derives it, at most 15 characters.
std::string xor_password(const std::string_view password) {
  const std::u16string password_u16 =
      util::string::string_to_u16string(password);
  std::string result;
  for (const char16_t c : password_u16) {
    if (result.size() == 15) {
      break;
    }
    const auto low = static_cast<std::uint8_t>(c & 0xff);
    result.push_back(
        static_cast<char>(low != 0 ? low : static_cast<std::uint8_t>(c >> 8)));
  }
  return result;
}

std::uint16_t
create_password_verifier_method1(const std::string_view password) {
  std::uint16_t verifier = 0;
  std::string password_array(1, static_cast<char>(password.size()));
  password_array += password;
  for (auto it = password_array.rbegin(); it != password_array.rend(); ++it) {
    const std::uint16_t intermediate1 = (verifier & 0x4000) == 0 ? 0 : 1;
    const auto intermediate2 =
        static_cast<std::uint16_t>((verifier * 2) & 0x7fff);
    verifier = static_cast<std::uint16_t>((intermediate1 | intermediate2) ^
                                          static_cast<std::uint8_t>(*it));
  }
  return verifier ^ 0xce4b;
}

std::uint16_t create_xor_key_method1(const std::string_view password) {
  std::uint16_t xor_key = initial_code.at(password.size() - 1);
  std::size_t current_element = 0x68;
  for (auto it = password.rbegin(); it != password.rend(); ++it) {
    auto c = static_cast<std::uint8_t>(*it);
    for (std::uint32_t i = 0; i < 7; ++i) {
      if ((c & 0x40) != 0) {
        xor_key ^= xor_matrix.at(current_element);
      }
      c = static_cast<std::uint8_t>(c * 2);
      --current_element;
    }
  }
  return xor_key;
}

std::uint8_t xor_ror(const std::uint8_t byte1, const std::uint8_t byte2) {
  const auto value = static_cast<std::uint8_t>(byte1 ^ byte2);
  return static_cast<std::uint8_t>((value >> 1) | (value << 7));
}

crypto::XorArray create_xor_array_method1(const std::string_view password,
                                          const std::uint16_t xor_key) {
  const auto high = static_cast<std::uint8_t>(xor_key >> 8);
  const auto low = static_cast<std::uint8_t>(xor_key & 0xff);
  const auto at = [&password](const std::size_t i) {
    return static_cast<std::uint8_t>(password[i]);
  };

  crypto::XorArray result{};
  std::size_t index = password.size();
  if (index % 2 == 1) {
    result[index] = xor_ror(pad_array[0], high);
    --index;
    result[index] = xor_ror(at(password.size() - 1), low);
  }
  while (index > 0) {
    --index;
    result[index] = xor_ror(at(index), high);
    --index;
    result[index] = xor_ror(at(index), low);
  }
  index = 15;
  std::size_t pad_index = 15 - password.size();
  while (pad_index > 0) {
    result[index] = xor_ror(pad_array[pad_index], high);
    --index;
    --pad_index;
    result[index] = xor_ror(pad_array[pad_index], low);
    --index;
    --pad_index;
  }
  return result;
}

} // namespace

crypto::Rc4Keys::Rc4Keys(const Kind kind, std::string password_hash,
                         const std::uint32_t key_size)
    : m_kind{kind}, m_password_hash{std::move(password_hash)},
      m_key_size{key_size} {}

std::string crypto::Rc4Keys::block_key(const std::uint32_t block) const {
  if (m_kind == Kind::rc4) {
    // [MS-OFFCRYPTO] 2.3.6.2.
    return internal::crypto::util::md5(m_password_hash.substr(0, 5) +
                                       le32(block));
  }
  // [MS-OFFCRYPTO] 2.3.5.2: a 40-bit key is padded with zeros to 128 bits.
  std::string key = internal::crypto::util::sha1(m_password_hash + le32(block))
                        .substr(0, m_key_size);
  if (m_key_size == 5) {
    key.resize(16, '\0');
  }
  return key;
}

std::string crypto::Rc4Keys::keystream(const std::size_t size,
                                       const std::size_t block_size) const {
  std::string result;
  result.reserve(size + block_size);
  const std::string zeros(block_size, '\0');
  for (std::uint32_t block = 0; result.size() < size; ++block) {
    result += internal::crypto::util::rc4(block_key(block), zeros);
  }
  result.resize(size);
  return result;
}

crypto::Rc4Encryption::Rc4Encryption(const std::string_view data) {
  util::byte_string::Reader reader(data);
  const auto major = reader.read<std::uint16_t>();
  const auto minor = reader.read<std::uint16_t>();

  if (major == 1 && minor == 1) {
    // [MS-OFFCRYPTO] 2.3.6.1.
    m_kind = Rc4Keys::Kind::rc4;
    m_salt = read_string(reader, 16);
    m_encrypted_verifier = read_string(reader, 16);
    m_encrypted_verifier_hash = read_string(reader, 16);
    return;
  }
  if ((major != 2 && major != 3 && major != 4) || minor != 2) {
    throw MsUnsupportedCryptoAlgorithm();
  }

  // [MS-OFFCRYPTO] 2.3.5.1, 2.3.2 and 2.3.3.
  m_kind = Rc4Keys::Kind::crypto_api;
  reader.skip(4); // a copy of EncryptionHeader.Flags
  const auto header_size = reader.read<std::uint32_t>();
  const std::string header = read_string(reader, header_size);
  util::byte_string::Reader header_reader(header);
  header_reader.skip(8); // Flags, SizeExtra
  const auto alg_id = header_reader.read<std::uint32_t>();
  const auto alg_id_hash = header_reader.read<std::uint32_t>();
  const auto key_bits = header_reader.read<std::uint32_t>();
  if ((alg_id != 0x6801 && alg_id != 0) ||
      (alg_id_hash != 0x8004 && alg_id_hash != 0)) {
    throw MsUnsupportedCryptoAlgorithm();
  }
  // KeySize 0 means 40 bits.
  const std::uint32_t effective_key_bits = key_bits == 0 ? 40 : key_bits;
  if (effective_key_bits < 40 || effective_key_bits > 128 ||
      effective_key_bits % 8 != 0) {
    throw MsUnsupportedCryptoAlgorithm();
  }
  m_key_size = effective_key_bits / 8;

  if (reader.read<std::uint32_t>() != 16) {
    throw std::runtime_error("oldms: bad RC4 CryptoAPI salt size");
  }
  m_salt = read_string(reader, 16);
  m_encrypted_verifier = read_string(reader, 16);
  if (reader.read<std::uint32_t>() != 20) {
    throw std::runtime_error("oldms: bad RC4 CryptoAPI verifier hash size");
  }
  m_encrypted_verifier_hash = read_string(reader, 20);
}

std::optional<crypto::Rc4Keys>
crypto::Rc4Encryption::unlock(const std::string_view password) const {
  std::string password_hash;
  if (m_kind == Rc4Keys::Kind::rc4) {
    // [MS-OFFCRYPTO] 2.3.6.2.
    const std::string h0 =
        internal::crypto::util::md5(password_bytes(password));
    std::string intermediate;
    for (std::uint32_t i = 0; i < 16; ++i) {
      intermediate += h0.substr(0, 5);
      intermediate += m_salt;
    }
    password_hash = internal::crypto::util::md5(intermediate);
  } else {
    // [MS-OFFCRYPTO] 2.3.5.2.
    password_hash =
        internal::crypto::util::sha1(m_salt + password_bytes(password));
  }
  Rc4Keys keys(m_kind, std::move(password_hash), m_key_size);

  // [MS-OFFCRYPTO] 2.3.5.6 and 2.3.6.4: one RC4 stream for both fields.
  const std::string decrypted = internal::crypto::util::rc4(
      keys.block_key(0), m_encrypted_verifier + m_encrypted_verifier_hash);
  const std::string verifier = decrypted.substr(0, 16);
  const std::string verifier_hash = decrypted.substr(16);
  const std::string expected_hash =
      m_kind == Rc4Keys::Kind::rc4 ? internal::crypto::util::md5(verifier)
                                   : internal::crypto::util::sha1(verifier);
  if (expected_hash != verifier_hash) {
    return std::nullopt;
  }
  return keys;
}

std::optional<crypto::XorArray>
crypto::unlock_xor_method1(const std::uint16_t key,
                           const std::uint16_t verifier,
                           const std::string_view password) {
  const std::string ansi_password = xor_password(password);
  if (ansi_password.empty() ||
      create_password_verifier_method1(ansi_password) != verifier) {
    return std::nullopt;
  }
  const std::uint16_t xor_key = create_xor_key_method1(ansi_password);
  if (xor_key != key) {
    return std::nullopt;
  }
  return create_xor_array_method1(ansi_password, xor_key);
}

void crypto::decrypt_xor_method1(const XorArray &xor_array, std::size_t index,
                                 char *data, const std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    const auto value =
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(data[i]) ^
                                  xor_array[index % xor_array.size()]);
    data[i] = static_cast<char>((value >> 5) | (value << 3));
    ++index;
  }
}

} // namespace odr::internal::oldms
