#include <string>
#include "gtest/gtest.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

TEST(MicrosoftBackTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/Desktop/odr/03_smpldap.docx";
    const std::string password = "password";
    const std::string output = "/home/andreas/Desktop/odr/edited_pre.html";
    const std::string backInput = "/home/andreas/Desktop/odr/edited.html";
    const std::string backOutput = "/home/andreas/Desktop/odr/edited.docx";

    odr::TranslationConfig config = {};
    config.editable = true;
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::TranslationHelper translator;
    translator.openMicrosoft(input);
    translator.decrypt(password);
    translator.translate(output, config);

    translator.backTranslate(backInput, backOutput);
}
