#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "MicrosoftOpenXmlFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

TEST(MicrosoftTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/Desktop/odr/03_smpldap.docx";
    const std::string output = "../../test/test.html";
    const std::string password = "password";

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::TranslationHelper translator;
    translator.openMicrosoft(input);
    translator.decrypt(password);
    translator.translate(output, config);
}
