#include <string>
#include "gtest/gtest.h"
#include "OpenDocumentFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

TEST(OpenDocumentTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/workspace/OpenDocument.test/files/spreadsheet/efficiency-big-2.ods";
    input = "/home/andreas/Desktop/odr/test.odp";
    input = "../../test/encrypted.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-undefined-1$password$.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-exception-1$password$.odt";
    input = "/home/andreas/Desktop/odr/file_example_ODP_1MB.odp";
    input = "/home/andreas/Desktop/odr/tuesday_d6.odp";
    input = "/home/andreas/Desktop/odr/ruski.odt";
    input = "/home/andreas/Desktop/odr/megatest.odt";
    input = "/home/andreas/Desktop/odr/Vektoranalysis Zusammenfassung.odt";
    const std::string output = "../../test/test.html";
    const std::string password = "password";

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::TranslationHelper translator;
    translator.open(input);
    translator.decrypt(password);
    translator.translate(output, config);
}
