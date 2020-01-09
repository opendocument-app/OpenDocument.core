#include <string>
#include "gtest/gtest.h"
#include "odr/TranslationConfig.h"
#include "odr/OpenDocumentReader.h"

TEST(MicrosoftTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/Desktop/odr/ruski.xlsx";
    input = "/home/andreas/Desktop/odr/tuesday_d6.pptx";
    input = "/home/andreas/Desktop/odr/03_smpldap.docx";
    const std::string output = "../../test/test.html";
    const std::string password = "password";

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::OpenDocumentReader translator;
    translator.openMicrosoft(input);
    translator.decrypt(password);
    translator.translate(output, config);
}
