#include <string>
#include <fstream>
#include <access/ZipStorage.h>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <access/CfbStorage.h>
#include <ooxml/OfficeOpenXmlCrypto.h>

TEST(MsCfbReader, open) {
    std::string input;
    input = "../../test/encrypted.docx";
    input = "../../test/encrypted.doc";
    input = "../../test/empty.doc";

    odr::CfbReader reader(input);
    reader.visit([&](const auto &p) {
        LOG(INFO) << p << " " << reader.size(p);
    });
}

TEST(MsCfbReader, read) {
    std::string input;
    input = "../../test/encrypted.docx";

    odr::CfbReader reader(input);
    std::string content(reader.size("EncryptionInfo"), ' ');
    reader.read("EncryptionInfo")->read(&content[0], content.size());
    LOG(INFO) << content;
}

TEST(MsCfbReader, decrypt) {
    std::string input;
    input = "../../test/encrypted.docx";

    odr::CfbReader reader(input);
    std::string encryptionInfo(reader.size("EncryptionInfo"), ' ');
    reader.read("EncryptionInfo")->read(&encryptionInfo[0], encryptionInfo.size());
    std::string encryptedPackage(reader.size("EncryptedPackage"), ' ');
    reader.read("EncryptedPackage")->read(&encryptedPackage[0], encryptedPackage.size());

    const odr::OfficeOpenXmlCrypto::Util cryptoUtil(encryptionInfo);
    const auto key = cryptoUtil.deriveKey("password");
    EXPECT_TRUE(cryptoUtil.verify(key));
    const auto decrypted = cryptoUtil.decrypt(encryptedPackage, key);

    odr::ZipReader zip(decrypted.data(), decrypted.size());
    zip.visit([](const auto &p) { LOG(INFO) << p; });
}
