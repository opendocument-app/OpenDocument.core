#ifdef ODR_CRYPTO
#include "CryptoUtil.h"
#include "cryptopp/hex.h"
#include "cryptopp/filters.h"
#include "cryptopp/base64.h"
#include "cryptopp/pwdbased.h"
#include "cryptopp/sha.h"
#include "cryptopp/modes.h"
#include "cryptopp/aes.h"
#include "cryptopp/zinflate.h"
#include "OpenDocumentFile.h"

namespace odr {

typedef unsigned char byte;

std::string CryptoUtil::base64decode(const std::string &in) {
    std::string out;
    CryptoPP::Base64Decoder b(new CryptoPP::StringSink(out));
    b.Put((const byte *) in.data(), in.size());
    b.MessageEnd();
    return out;
}

std::string CryptoUtil::sha256(const std::string &in) {
    byte out[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::SHA256().CalculateDigest(out, (byte *) in.data(), in.size());
    return std::string((char *) out, CryptoPP::SHA256::DIGESTSIZE);
}

std::string CryptoUtil::deriveKey(const std::size_t keySize, const std::string &startKey,
                             const std::string &salt, const std::size_t iterationCount) {
    std::string result(keySize, '\0');
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA1> pbkdf2;
    pbkdf2.DeriveKey((byte *) result.data(), result.size(), false, (byte *) startKey.data(), startKey.size(),
                     (byte *) salt.data(), salt.size(), iterationCount);
    return result;
}

std::string CryptoUtil::decryptAes(const std::string &key, const std::string &iv, const std::string &input) {
    std::string result;
    CryptoPP::AES::Decryption aesDecryption((byte *) key.data(), key.size());
    CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, (byte *) iv.data());
    CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(result),
                                                      CryptoPP::BlockPaddingSchemeDef::NO_PADDING);
    stfDecryptor.Put((byte *) input.data(), input.size());
    stfDecryptor.MessageEnd();
    return result;
}

std::string CryptoUtil::deriveKeyAndDecrypt(const OpenDocumentEntry &entry, const std::string &startKey,
                                       const std::string &input) {
    std::string derivedKey = deriveKey(entry.keySize, startKey, entry.keySalt, entry.keyIterationCount);
    return decryptAes(derivedKey, entry.initialisationVector, input);
}

std::string CryptoUtil::inflate(const std::string &input) {
    std::string result;
    CryptoPP::Inflator inflator(new CryptoPP::StringSink(result));
    inflator.Put((byte *) input.data(), input.size());
    inflator.MessageEnd();
    return result;
}

}

#endif
