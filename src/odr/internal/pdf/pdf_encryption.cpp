#include <odr/internal/pdf/pdf_encryption.hpp>

#include <odr/internal/crypto/crypto_util.hpp>

#include <algorithm>
#include <array>

namespace odr::internal::pdf {

namespace {

constexpr std::size_t aes_block = 16;

/// The 32-byte password padding constant (ISO 32000-1 Algorithm 2, step a).
const std::string padding = [] {
  static constexpr std::array<std::uint8_t, 32> bytes = {
      0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E,
      0x56, 0xFF, 0xFA, 0x01, 0x08, 0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68,
      0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};
  return std::string(reinterpret_cast<const char *>(bytes.data()),
                     bytes.size());
}();

/// `p` as a signed 32-bit little-endian value (Algorithm 2, step d).
std::string int32_le(const std::int64_t p) {
  const auto u = static_cast<std::uint32_t>(static_cast<std::int32_t>(p));
  std::string out(4, '\0');
  for (std::size_t i = 0; i < 4; ++i) {
    out[i] = static_cast<char>((u >> (8 * i)) & 0xff);
  }
  return out;
}

/// XOR every byte of `key` with the constant `x` (Algorithms 5/7, R 3+).
std::string xor_key(std::string key, const std::uint8_t x) {
  for (char &c : key) {
    c = static_cast<char>(static_cast<std::uint8_t>(c) ^ x);
  }
  return key;
}

/// Pad/truncate a password to 32 bytes with the padding constant (Algorithm 2,
/// step a).
std::string pad_password(const std::string &password) {
  std::string pw =
      password.substr(0, std::min<std::size_t>(password.size(), 32));
  pw += padding.substr(0, 32 - pw.size());
  return pw;
}

/// Strip PKCS#7 padding (1-16 bytes) from decrypted AES data; leave the input
/// untouched if the trailer is not valid padding (lenient — real files vary).
std::string strip_pkcs7(std::string data) {
  if (data.empty()) {
    return data;
  }
  const auto n = static_cast<std::uint8_t>(data.back());
  if (n >= 1 && n <= aes_block && n <= data.size()) {
    data.resize(data.size() - n);
  }
  return data;
}

} // namespace

// Standard-security algorithms with no callers outside this file (the rest are
// declared in the header for known-answer tests).
namespace standard_security {

/// Algorithm 7: recover the (padded) user password from `/O` using an owner
/// password, for R 2-4.
std::string recover_user_password(const std::string &owner_password,
                                  const std::string &o, const std::int64_t r,
                                  const std::size_t key_length) {
  // Algorithm 7: derive the RC4 key from the owner password, then RC4-decrypt
  // /O to recover the user password.
  std::string hash = crypto::util::md5(pad_password(owner_password));
  if (r >= 3) {
    for (int i = 0; i < 50; ++i) {
      hash = crypto::util::md5(hash.substr(0, key_length));
    }
  }
  const std::string rc4_key = hash.substr(0, key_length);

  std::string user = o.substr(0, 32);
  if (r == 2) {
    return crypto::util::rc4(rc4_key, user);
  }
  for (int i = 19; i >= 0; --i) {
    user =
        crypto::util::rc4(xor_key(rc4_key, static_cast<std::uint8_t>(i)), user);
  }
  return user;
}

/// ISO 32000-2 Algorithm 2.B: the R 6 hardened hash. `udata` is empty for the
/// user password and the 48-byte `/U` value for the owner password.
std::string hash_r6(const std::string &password, const std::string &salt,
                    const std::string &udata) {
  // ISO 32000-2 Algorithm 2.B: SHA-256 seed, then rounds of AES-128-CBC over a
  // 64x-repeated block, the digest function chosen by (E mod 3). At least 64
  // rounds; stop once the last byte of E is small enough.
  std::string k = crypto::util::sha256(password + salt + udata);
  std::string e;
  for (int round = 0;
       round < 64 || static_cast<std::uint8_t>(e.back()) > round - 32;
       ++round) {
    std::string block;
    block.reserve(password.size() + k.size() + udata.size());
    block += password;
    block += k;
    block += udata;
    std::string k1;
    k1.reserve(block.size() * 64);
    for (int i = 0; i < 64; ++i) {
      k1 += block;
    }
    e = crypto::util::encrypt_aes_cbc(k.substr(0, 16), k.substr(16, 16), k1);

    int mod = 0; // the first 16 bytes of E as a big-endian integer, mod 3
    for (std::size_t i = 0; i < 16; ++i) {
      mod = (mod * 256 + static_cast<std::uint8_t>(e[i])) % 3;
    }
    if (mod == 0) {
      k = crypto::util::sha256(e);
    } else if (mod == 1) {
      k = crypto::util::sha384(e);
    } else {
      k = crypto::util::sha512(e);
    }
  }
  return k.substr(0, 32);
}

} // namespace standard_security

std::string standard_security::compute_key_r2_r4(
    const std::string &password, const std::string &o, const std::int64_t p,
    const std::string &id0, const std::int64_t r, const std::size_t key_length,
    const bool encrypt_metadata) {
  // Algorithm 2: MD5 over padded password + O + P + ID[0] (+ metadata salt),
  // then 50 re-hashes for R >= 3.
  std::string input =
      pad_password(password) + o.substr(0, 32) + int32_le(p) + id0;
  if (r >= 4 && !encrypt_metadata) {
    input.append(4, static_cast<char>(0xff)); // step (f)
  }
  std::string hash = crypto::util::md5(input);
  if (r >= 3) {
    for (int i = 0; i < 50; ++i) {
      hash = crypto::util::md5(hash.substr(0, key_length));
    }
  }
  return hash.substr(0, key_length);
}

std::string standard_security::compute_u_r2_r4(const std::string &key,
                                               const std::string &id0,
                                               const std::int64_t r) {
  if (r == 2) {
    // Algorithm 4: RC4 of the padding constant.
    return crypto::util::rc4(key, padding);
  }
  // Algorithm 5 (R >= 3): MD5(padding + ID[0]), RC4 with the key, then 19
  // further RC4 passes with the key XORed by the iteration number.
  std::string x = crypto::util::rc4(key, crypto::util::md5(padding + id0));
  for (std::uint8_t i = 1; i <= 19; ++i) {
    x = crypto::util::rc4(xor_key(key, i), x);
  }
  return x;
}

namespace {

/// Map a crypt-filter `/CFM` name to a decryption method.
std::optional<EncryptionMethod> cfm_method(const std::string &cfm) {
  if (cfm == "V2") {
    return EncryptionMethod::rc4;
  }
  if (cfm == "AESV2") {
    return EncryptionMethod::aes_v2;
  }
  if (cfm == "AESV3") {
    return EncryptionMethod::aes_v3;
  }
  if (cfm == "Identity") {
    return EncryptionMethod::none;
  }
  return std::nullopt;
}

/// Resolve the method named by `/StmF` or `/StrF` against the `/CF` dictionary
/// (V 4/5). The name `Identity` is implicit and need not appear in `/CF`.
std::optional<EncryptionMethod> resolve_crypt_filter(const Dictionary &encrypt,
                                                     const std::string &name) {
  if (name == "Identity") {
    return EncryptionMethod::none;
  }
  if (!encrypt.has_key("CF") || !encrypt["CF"].is_dictionary()) {
    return std::nullopt;
  }
  const Dictionary &cf = encrypt["CF"].as_dictionary();
  if (!cf.has_key(name) || !cf[name].is_dictionary()) {
    return std::nullopt;
  }
  const Dictionary &filter = cf[name].as_dictionary();
  if (!filter.has_key("CFM") || !filter["CFM"].is_name()) {
    return std::nullopt;
  }
  return cfm_method(filter["CFM"].as_name());
}

} // namespace

std::optional<Authenticator>
Authenticator::create(const Dictionary &encrypt, const std::string &file_id0) {
  // Only the standard security handler is supported.
  if (!encrypt.has_key("Filter") || !encrypt["Filter"].is_name() ||
      encrypt["Filter"].as_name() != "Standard") {
    return std::nullopt;
  }
  if (!encrypt.has_key("R") || !encrypt["R"].is_integer()) {
    return std::nullopt;
  }

  Authenticator d;
  d.m_v = encrypt.has_key("V") ? encrypt["V"].as_integer() : 0;
  d.m_r = encrypt["R"].as_integer();
  d.m_o = encrypt["O"].as_string();
  d.m_u = encrypt["U"].as_string();
  d.m_p = encrypt["P"].as_integer();
  d.m_id0 = file_id0;
  if (encrypt.has_key("EncryptMetadata") &&
      encrypt["EncryptMetadata"].is_bool()) {
    d.m_encrypt_metadata = encrypt["EncryptMetadata"].as_bool();
  }

  if (d.m_v >= 5) {
    if (!encrypt.has_key("OE") || !encrypt.has_key("UE")) {
      return std::nullopt;
    }
    d.m_oe = encrypt["OE"].as_string();
    d.m_ue = encrypt["UE"].as_string();
    d.m_key_length = 32;
    d.m_stream_method = EncryptionMethod::aes_v3;
    d.m_string_method = EncryptionMethod::aes_v3;
    if (d.m_r != 6) {
      return std::nullopt; // R 5 (deprecated interim revision) is out of scope
    }
    return d;
  }

  // V < 5: file key length in bytes (Length is in bits, default 40).
  d.m_key_length =
      d.m_r == 2
          ? 5
          : static_cast<std::size_t>((encrypt.has_key("Length")
                                          ? encrypt["Length"].as_integer()
                                          : 40) /
                                     8);

  if (d.m_v == 4) {
    // Crypt filters select the method for streams and strings; the defaults
    // are Identity (Table 20).
    const std::string stmf =
        encrypt.has_key("StmF") && encrypt["StmF"].is_name()
            ? encrypt["StmF"].as_name()
            : "Identity";
    const std::string strf =
        encrypt.has_key("StrF") && encrypt["StrF"].is_name()
            ? encrypt["StrF"].as_name()
            : "Identity";
    const auto stream_method = resolve_crypt_filter(encrypt, stmf);
    const auto string_method = resolve_crypt_filter(encrypt, strf);
    if (!stream_method || !string_method) {
      return std::nullopt; // unsupported crypt filter
    }
    d.m_stream_method = *stream_method;
    d.m_string_method = *string_method;
  } else {
    // V 1/2: RC4 throughout.
    d.m_stream_method = EncryptionMethod::rc4;
    d.m_string_method = EncryptionMethod::rc4;
  }

  return d;
}

std::optional<Decryptor>
Authenticator::authenticate(const std::string &password) const {
  using standard_security::hash_r6;

  if (m_r == 6) {
    // ISO 32000-2 Algorithms 11/12: validate against the salts in /U and /O,
    // then unwrap the file key from /UE or /OE with AES-256-CBC (zero IV).
    const std::string zero_iv(aes_block, '\0');

    if (hash_r6(password, m_u.substr(32, 8), {}) == m_u.substr(0, 32)) {
      const std::string ik = hash_r6(password, m_u.substr(40, 8), {});
      std::string key = crypto::util::decrypt_aes_cbc(ik, zero_iv, m_ue);
      return Decryptor(std::move(key), m_stream_method, m_string_method);
    }
    const std::string u48 = m_u.substr(0, 48);
    if (hash_r6(password, m_o.substr(32, 8), u48) == m_o.substr(0, 32)) {
      const std::string ik = hash_r6(password, m_o.substr(40, 8), u48);
      std::string key = crypto::util::decrypt_aes_cbc(ik, zero_iv, m_oe);
      return Decryptor(std::move(key), m_stream_method, m_string_method);
    }
    return std::nullopt;
  }

  // R 2-4. Try the password as the user password, then as the owner password.
  const std::size_t compare =
      m_r == 2 ? 32 : 16; // R 3+ compares the first 16 bytes of /U

  std::string key = standard_security::compute_key_r2_r4(
      password, m_o, m_p, m_id0, m_r, m_key_length, m_encrypt_metadata);
  if (standard_security::compute_u_r2_r4(key, m_id0, m_r).substr(0, compare) ==
      m_u.substr(0, compare)) {
    return Decryptor(std::move(key), m_stream_method, m_string_method);
  }

  const std::string user = standard_security::recover_user_password(
      password, m_o, m_r, m_key_length);
  key = standard_security::compute_key_r2_r4(user, m_o, m_p, m_id0, m_r,
                                             m_key_length, m_encrypt_metadata);
  if (standard_security::compute_u_r2_r4(key, m_id0, m_r).substr(0, compare) ==
      m_u.substr(0, compare)) {
    return Decryptor(std::move(key), m_stream_method, m_string_method);
  }

  return std::nullopt;
}

Decryptor::Decryptor(std::string key, EncryptionMethod stream_method,
                     EncryptionMethod string_method)
    : m_key(std::move(key)), m_stream_method(stream_method),
      m_string_method(string_method) {}

std::string Decryptor::object_key(const ObjectReference &reference,
                                  const EncryptionMethod method) const {
  // Algorithm 1 (V < 5): MD5 of the file key + low 3 bytes of the object id +
  // low 2 bytes of the generation (+ "sAlT" for AES), truncated to n+5 (<=16).
  std::string input = m_key;
  input += static_cast<char>(reference.id & 0xff);
  input += static_cast<char>((reference.id >> 8) & 0xff);
  input += static_cast<char>((reference.id >> 16) & 0xff);
  input += static_cast<char>(reference.gen & 0xff);
  input += static_cast<char>((reference.gen >> 8) & 0xff);
  if (method == EncryptionMethod::aes_v2) {
    input += "sAlT";
  }
  const std::size_t n = std::min<std::size_t>(m_key.size() + 5, 16);
  return crypto::util::md5(input).substr(0, n);
}

std::string Decryptor::decrypt(const ObjectReference &reference,
                               std::string data,
                               const EncryptionMethod method) const {
  if (method == EncryptionMethod::none) {
    return data;
  }

  if (method == EncryptionMethod::aes_v3) {
    // V 5: the file key is used directly; first 16 bytes are the IV.
    if (data.size() < aes_block) {
      return data;
    }
    return strip_pkcs7(crypto::util::decrypt_aes_cbc(
        m_key, data.substr(0, aes_block), data.substr(aes_block)));
  }

  const std::string key = object_key(reference, method);
  if (method == EncryptionMethod::aes_v2) {
    if (data.size() < aes_block) {
      return data;
    }
    return strip_pkcs7(crypto::util::decrypt_aes_cbc(
        key, data.substr(0, aes_block), data.substr(aes_block)));
  }

  return crypto::util::rc4(key, data); // Method::rc4
}

std::string Decryptor::decrypt_stream(const ObjectReference &reference,
                                      std::string data) const {
  return decrypt(reference, std::move(data), m_stream_method);
}

std::string Decryptor::decrypt_string(const ObjectReference &reference,
                                      std::string data) const {
  return decrypt(reference, std::move(data), m_string_method);
}

} // namespace odr::internal::pdf
