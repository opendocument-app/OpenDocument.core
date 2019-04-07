#include <string>
#include "gtest/gtest.h"
#include "tinyxml2.h"
#include "cryptopp/hex.h"
#include "cryptopp/filters.h"
#include "cryptopp/base64.h"
#include "cryptopp/pwdbased.h"
#include "cryptopp/sha.h"
#include "cryptopp/modes.h"
#include "cryptopp/aes.h"
#include "cryptopp/zinflate.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"

typedef unsigned char byte;

std::string base64decode(const std::string &in) {
    std::string out;
    CryptoPP::Base64Decoder b(new CryptoPP::StringSink(out));
    b.Put((const byte *) in.data(), in.size());
    b.MessageEnd();
    return out;
}

std::string sha256(const std::string &in) {
    byte out[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::SHA256().CalculateDigest(out, (byte *) in.data(), in.size());
    return std::string((char *) out, CryptoPP::SHA256::DIGESTSIZE);
}

TEST(DecryptionTest, open) {
    const std::string path = "../../test/encrypted.odt";
    const std::string password = "password";

    auto odf = odr::OpenDocumentFile::create();
    odf->open(path);

    auto manifest = odf->loadXML("META-INF/manifest.xml");
    tinyxml2::XMLElement *e = manifest
            ->FirstChildElement("manifest:manifest")
            ->FirstChildElement("manifest:file-entry");
    while (e != nullptr) {
        std::string fullPath = e->FindAttribute("manifest:full-path")->Value();
        LOG(INFO) << fullPath;

        tinyxml2::XMLElement *crypto = e->FirstChildElement("manifest:encryption-data");
        if (crypto != nullptr) {
            tinyxml2::XMLElement *algo = crypto->FirstChildElement("manifest:algorithm");
            tinyxml2::XMLElement *key = crypto->FirstChildElement("manifest:key-derivation");
            tinyxml2::XMLElement *start = crypto->FirstChildElement("manifest:start-key-generation");

            std::string checksumType = crypto->FindAttribute("manifest:checksum-type")->Value();
            std::string checksum = crypto->FindAttribute("manifest:checksum")->Value();
            std::string algorithmName = algo->FindAttribute("manifest:algorithm-name")->Value();
            std::string initialisationVector = algo->FindAttribute("manifest:initialisation-vector")->Value();
            std::string keyDerivationName = key->FindAttribute("manifest:key-derivation-name")->Value();
            std::int64_t keySize = key->FindAttribute("manifest:key-size")->Int64Value();
            std::int64_t keyIterationCount = key->FindAttribute("manifest:iteration-count")->Int64Value();
            std::string keySalt = key->FindAttribute("manifest:salt")->Value();
            std::string startKeyGenerationName = start->FindAttribute("manifest:start-key-generation-name")->Value();
            std::string startKeySize = start->FindAttribute("manifest:key-size")->Value();

            LOG(INFO) << "algo " << algorithmName;
            LOG(INFO) << "key " << keyDerivationName << " " << keySize << " " << keyIterationCount;
            LOG(INFO) << "start " << startKeyGenerationName << " " << startKeySize;

            std::string cs = base64decode(checksum);
            std::string iv = base64decode(initialisationVector);
            std::string salt = base64decode(keySalt);
            std::string startKey = sha256(password);

            std::string derived(keySize, '\0');
            CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA1> pbkdf2;
            pbkdf2.DeriveKey((byte *) derived.data(), derived.size(), false, (byte *) startKey.data(), startKey.size(),
                    (byte *) salt.data(), salt.size(), keyIterationCount);

            std::string ciphertext = odf->loadText(fullPath);
            std::string decryptedtext;
            std::string inflatedtext;

            CryptoPP::AES::Decryption aesDecryption((byte *) derived.data(), derived.size());
            CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, (byte *) iv.data());
            CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(decryptedtext), CryptoPP::BlockPaddingSchemeDef::NO_PADDING);
            stfDecryptor.Put((byte *) ciphertext.data(), ciphertext.size());
            stfDecryptor.MessageEnd();

            CryptoPP::Inflator inflator(new CryptoPP::StringSink(inflatedtext));
            inflator.Put((byte *) decryptedtext.data(), decryptedtext.size());
            inflator.MessageEnd();

            LOG(INFO) << inflatedtext;

            // TODO: check hash
        }

        e = e->NextSiblingElement("manifest:file-entry");
    }
}
