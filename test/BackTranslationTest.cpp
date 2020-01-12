#include <string>
#include "gtest/gtest.h"
#include "odr/TranslationConfig.h"
#include "odr/OpenDocumentReader.h"
#include "io/FileUtil.h"

TEST(OpenDocumentBackTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/Desktop/odr/03_smpldap.docx";
    input = "/home/andreas/workspace/OpenDocument.test/odt/03_smpldap.odt";
    const std::string password = "password";
    const std::string output = "/home/andreas/Desktop/odr/edited_pre.html";
    const std::string backInput = "/home/andreas/Downloads/test.json";
    const std::string backOutput = "/home/andreas/Desktop/odr/edited.odt";

    odr::TranslationConfig config = {};
    config.editable = true;
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::OpenDocumentReader translator;
    translator.open(input);
    translator.decrypt(password);
    translator.translate(output, config);

    const std::string backDiff = odr::FileUtil::read(backInput);
    translator.backTranslate(backDiff, backOutput);
}
