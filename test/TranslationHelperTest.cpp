#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

TEST(TranslationHelperTest, translate) {
    std::string input;
    input = "/home/andreas/workspace/OpenDocument.test/files/spreadsheet/efficiency-big-2.ods";
    input = "/home/andreas/Desktop/odr/test.odp";
    input = "../../test/encrypted.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-undefined-1$password$.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-exception-1$password$.odt";
    input = "/home/andreas/Desktop/odr/ruski.odt";
    input = "/home/andreas/Desktop/odr/tuesday_d6.odp";
    const std::string output = "../../test/test.html";
    const std::string password = "password";

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;
    auto translator = odr::TranslationHelper::create();
    translator->open(input);
    translator->decrypt(password);
    translator->translate(output, config);
}
