#include <odr/internal/crypto/crypto_util.hpp>

#include <cstdint>

#include <cryptopp/aes.h>
#include <cryptopp/base64.h>
#include <cryptopp/blowfish.h>
#include <cryptopp/des.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <cryptopp/zinflate.h>
#include <cryptopp/zlib.h>

#include <argon2.h>

namespace odr::internal::crypto {

using byte = std::uint8_t;

std::string util::base64_encode(const std::string &in) {
  std::string out;
  CryptoPP::Base64Encoder b(new CryptoPP::StringSink(out), false);
  b.Put(reinterpret_cast<const byte *>(in.data()), in.size());
  b.MessageEnd();
  return out;
}

std::string util::base64_decode(const std::string &in) {
  std::string out;
  CryptoPP::Base64Decoder b(new CryptoPP::StringSink(out));
  b.Put(reinterpret_cast<const byte *>(in.data()), in.size());
  b.MessageEnd();
  return out;
}

std::string util::sha1(const std::string &in) {
  byte out[CryptoPP::SHA1::DIGESTSIZE];
  CryptoPP::SHA1().CalculateDigest(
      out, reinterpret_cast<const byte *>(in.data()), in.size());
  return std::string(reinterpret_cast<char *>(out), CryptoPP::SHA1::DIGESTSIZE);
}

std::string util::sha256(const std::string &in) {
  byte out[CryptoPP::SHA256::DIGESTSIZE];
  CryptoPP::SHA256().CalculateDigest(
      out, reinterpret_cast<const byte *>(in.data()), in.size());
  return std::string(reinterpret_cast<char *>(out),
                     CryptoPP::SHA256::DIGESTSIZE);
}

std::string util::pbkdf2(std::size_t key_size, const std::string &start_key,
                         const std::string &salt, std::size_t iteration_count) {
  std::string result(key_size, '\0');
  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA1> pbkdf2;
  pbkdf2.DeriveKey(reinterpret_cast<byte *>(result.data()), result.size(),
                   false, reinterpret_cast<const byte *>(start_key.data()),
                   start_key.size(),
                   reinterpret_cast<const byte *>(salt.data()), salt.size(),
                   iteration_count);
  return result;
}

std::string util::argon2id(std::size_t key_size, const std::string &start_key,
                           const std::string &salt, std::size_t iteration_count,
                           std::size_t memory, std::size_t lanes) {
  std::string result(key_size, '\0');
  argon2id_hash_raw(iteration_count, memory, lanes, start_key.data(),
                    start_key.size(), salt.data(), salt.size(), result.data(),
                    result.size());
  return result;
}

std::string util::decrypt_aes_ecb(const std::string &key,
                                  const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption decryption;
  decryption.SetKey(reinterpret_cast<const byte *>(key.data()), key.size());
  decryption.ProcessData(reinterpret_cast<byte *>(result.data()),
                         reinterpret_cast<const byte *>(input.data()),
                         input.size());
  return result;
}

std::string util::decrypt_aes_cbc(const std::string &key, const std::string &iv,
                                  const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryption;
  decryption.SetKeyWithIV(reinterpret_cast<const byte *>(key.data()),
                          key.size(), reinterpret_cast<const byte *>(iv.data()),
                          iv.size());
  decryption.ProcessData(reinterpret_cast<byte *>(result.data()),
                         reinterpret_cast<const byte *>(input.data()),
                         input.size());
  return result;
}

std::string util::decrypt_aes_gcm(const std::string &key, const std::string &iv,
                                  const std::string &input) {
  if (std::strncmp(iv.data(), input.data(), iv.size()) != 0) {
    throw std::runtime_error("IV mismatch");
  }

  std::string result(input.size(), '\0');

  std::size_t iv_size = iv.size();
  std::size_t mac_size = 16;
  std::size_t cipher_size = input.size() - iv_size - mac_size;
  byte *message = reinterpret_cast<byte *>(result.data());
  const byte *mac =
      reinterpret_cast<const byte *>(input.data() + input.size() - mac_size);
  const byte *iv_ = reinterpret_cast<const byte *>(input.data());
  const byte *cipher = reinterpret_cast<const byte *>(input.data() + iv_size);

  CryptoPP::GCM<CryptoPP::AES>::Decryption decryption;
  decryption.SetKeyWithIV(reinterpret_cast<const byte *>(key.data()),
                          key.size(), reinterpret_cast<const byte *>(iv.data()),
                          iv.size());
  bool check = decryption.DecryptAndVerify(message, mac, mac_size, iv_, iv_size,
                                           nullptr, 0, cipher, cipher_size);

  if (!check) {
    throw std::runtime_error("GCM decryption failed");
  }

  return result;
}

std::string util::decrypt_triple_des(const std::string &key,
                                     const std::string &iv,
                                     const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CBC_Mode<CryptoPP::DES_EDE3>::Decryption decryption;
  decryption.SetKeyWithIV(reinterpret_cast<const byte *>(key.data()),
                          key.size(), reinterpret_cast<const byte *>(iv.data()),
                          iv.size());
  decryption.ProcessData(reinterpret_cast<byte *>(result.data()),
                         reinterpret_cast<const byte *>(input.data()),
                         input.size());
  return result;
}

std::string util::decrypt_blowfish(const std::string &key,
                                   const std::string &iv,
                                   const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CFB_Mode<CryptoPP::Blowfish>::Decryption decryption;
  decryption.SetKeyWithIV(reinterpret_cast<const byte *>(key.data()),
                          key.size(), reinterpret_cast<const byte *>(iv.data()),
                          iv.size());
  decryption.ProcessData(reinterpret_cast<byte *>(result.data()),
                         reinterpret_cast<const byte *>(input.data()),
                         input.size());
  return result;
}

namespace {
/// discard non deflated content caused by padding
class MyInflator final : public CryptoPP::Inflator {
public:
  explicit MyInflator(BufferedTransformation *attachment = nullptr)
      : Inflator(attachment, false, -1) {}

  std::uint32_t GetPadding() const { return m_padding; }

protected:
  void ProcessPoststreamTail() final {
    m_padding = m_inQueue.CurrentSize();
    m_inQueue.Clear();
  }

private:
  std::uint32_t m_padding{0};
};
} // namespace

std::string util::inflate(const std::string &input) {
  std::string result;
  MyInflator inflator(new CryptoPP::StringSink(result));
  inflator.Put(reinterpret_cast<const byte *>(input.data()), input.size());
  inflator.MessageEnd();
  return result;
}

std::size_t util::padding(const std::string &input) {
  MyInflator inflator;
  inflator.Put(reinterpret_cast<const byte *>(input.data()), input.size());
  inflator.MessageEnd();
  return inflator.GetPadding();
}

std::string util::zlib_inflate(const std::string &input) {
  std::string result;
  CryptoPP::ZlibDecompressor inflator(new CryptoPP::StringSink(result));
  inflator.Put(reinterpret_cast<const byte *>(input.data()), input.size());
  inflator.MessageEnd();
  return result;
}

} // namespace odr::internal::crypto
