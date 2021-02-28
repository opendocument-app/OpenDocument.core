#include <aes.h>
#include <base64.h>
#include <blowfish.h>
#include <crypto/crypto_util.h>
#include <des.h>
#include <filters.h>
#include <modes.h>
#include <pwdbased.h>
#include <sha.h>
#include <zinflate.h>

namespace odr::crypto {

typedef unsigned char byte;

std::string Util::base64_encode(const std::string &in) {
  std::string out;
  CryptoPP::Base64Encoder b(new CryptoPP::StringSink(out), false);
  b.Put((const byte *)in.data(), in.size());
  b.MessageEnd();
  return out;
}

std::string Util::base64_decode(const std::string &in) {
  std::string out;
  CryptoPP::Base64Decoder b(new CryptoPP::StringSink(out));
  b.Put((const byte *)in.data(), in.size());
  b.MessageEnd();
  return out;
}

std::string Util::sha1(const std::string &in) {
  byte out[CryptoPP::SHA1::DIGESTSIZE];
  CryptoPP::SHA1().CalculateDigest(out, (byte *)in.data(), in.size());
  return std::string((char *)out, CryptoPP::SHA1::DIGESTSIZE);
}

std::string Util::sha256(const std::string &in) {
  byte out[CryptoPP::SHA256::DIGESTSIZE];
  CryptoPP::SHA256().CalculateDigest(out, (byte *)in.data(), in.size());
  return std::string((char *)out, CryptoPP::SHA256::DIGESTSIZE);
}

std::string Util::pbkdf2(const std::size_t keySize, const std::string &startKey,
                         const std::string &salt,
                         const std::size_t iterationCount) {
  std::string result(keySize, '\0');
  CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA1> pbkdf2;
  pbkdf2.DeriveKey((byte *)result.data(), result.size(), false,
                   (byte *)startKey.data(), startKey.size(),
                   (byte *)salt.data(), salt.size(), iterationCount);
  return result;
}

std::string Util::decrypt_AES(const std::string &key,
                              const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption decryptor;
  decryptor.SetKey((byte *)key.data(), key.size());
  decryptor.ProcessData((byte *)result.data(), (byte *)input.data(),
                        input.size());
  return result;
}

std::string Util::decrypt_AES(const std::string &key, const std::string &iv,
                              const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption decryptor;
  decryptor.SetKeyWithIV((byte *)key.data(), key.size(), (byte *)iv.data(),
                         iv.size());
  decryptor.ProcessData((byte *)result.data(), (byte *)input.data(),
                        input.size());
  return result;
}

std::string Util::decrypt_TripleDES(const std::string &key,
                                    const std::string &iv,
                                    const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CBC_Mode<CryptoPP::DES_EDE3>::Decryption decryptor;
  decryptor.SetKeyWithIV((byte *)key.data(), key.size(), (byte *)iv.data(),
                         iv.size());
  decryptor.ProcessData((byte *)result.data(), (byte *)input.data(),
                        input.size());
  return result;
}

std::string Util::decrypt_Blowfish(const std::string &key,
                                   const std::string &iv,
                                   const std::string &input) {
  std::string result(input.size(), '\0');
  CryptoPP::CFB_Mode<CryptoPP::Blowfish>::Decryption decryptor;
  decryptor.SetKeyWithIV((byte *)key.data(), key.size(), (byte *)iv.data(),
                         iv.size());
  decryptor.ProcessData((byte *)result.data(), (byte *)input.data(),
                        input.size());
  return result;
}

namespace {
// discard non deflated content caused by padding
class MyInflator final : public CryptoPP::Inflator {
public:
  MyInflator(BufferedTransformation *attachment = nullptr, bool repeat = false,
             int auto_signal_propagation = -1)
      : Inflator(attachment, repeat, auto_signal_propagation) {}

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

std::string Util::inflate(const std::string &input) {
  std::string result;
  MyInflator inflator(new CryptoPP::StringSink(result));
  inflator.Put((byte *)input.data(), input.size());
  inflator.MessageEnd();
  return result;
}

std::size_t Util::padding(const std::string &input) {
  MyInflator inflator;
  inflator.Put((byte *)input.data(), input.size());
  inflator.MessageEnd();
  return inflator.GetPadding();
}

} // namespace odr::crypto
