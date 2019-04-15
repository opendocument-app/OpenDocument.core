#include <string>
#include "gtest/gtest.h"
#include "tinyxml2.h"
#include "cryptopp/filters.h"
#include "cryptopp/pwdbased.h"
#include "cryptopp/sha.h"
#include "cryptopp/modes.h"
#include "cryptopp/aes.h"
#include "cryptopp/zinflate.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"

typedef unsigned char byte;

static std::string sha256(const std::string &in) {
    byte out[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::SHA256().CalculateDigest(out, (byte *) in.data(), in.size());
    return std::string((char *) out, CryptoPP::SHA256::DIGESTSIZE);
}

TEST(DecryptionTest, open) {
    const std::string path = "../../test/encrypted.odt";
    const std::string password = "password";

    std::string startKey = sha256(password);

    auto odf = odr::OpenDocumentFile::create();
    odf->open(path);

    for (auto &&entry : odf->getEntries()) {
        if (!entry.second.encrypted) {
            continue;
        }

        std::string derived(entry.second.keySize, '\0');
        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA1> pbkdf2;
        pbkdf2.DeriveKey((byte *) derived.data(), derived.size(), false, (byte *) startKey.data(), startKey.size(),
                         (byte *) entry.second.keySalt.data(), entry.second.keySalt.size(), entry.second.keyIterationCount);

        std::string ciphertext = odf->loadText(entry.first);
        std::string decryptedtext;
        std::string inflatedtext;

        CryptoPP::AES::Decryption aesDecryption((byte *) derived.data(), derived.size());
        CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, (byte *) entry.second.initialisationVector.data());
        CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(decryptedtext), CryptoPP::BlockPaddingSchemeDef::NO_PADDING);
        stfDecryptor.Put((byte *) ciphertext.data(), ciphertext.size());
        stfDecryptor.MessageEnd();

        CryptoPP::Inflator inflator(new CryptoPP::StringSink(inflatedtext));
        inflator.Put((byte *) decryptedtext.data(), decryptedtext.size());
        inflator.MessageEnd();

        LOG(INFO) << inflatedtext;

        // TODO: check hash
    }
}
