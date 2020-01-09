#include <string>
#include "gtest/gtest.h"
#include "odr/TranslationConfig.h"
#include "odr/OpenDocumentReader.h"
#include "io/OpenDocumentFile.h"

TEST(OpenDocumentTranslationTest, translate) {
    std::string input;
    input = "/home/andreas/Desktop/odr/test.odp";
    input = "../../test/encrypted.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-undefined-1$password$.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/text/encrypted-exception-1$password$.odt";
    input = "/home/andreas/Desktop/odr/file_example_ODP_1MB.odp";
    input = "/home/andreas/Desktop/odr/tuesday_d6.odp";
    input = "/home/andreas/Desktop/odr/ruski.odt";
    input = "/home/andreas/Desktop/odr/Vektoranalysis Zusammenfassung.odt";
    input = "/home/andreas/Desktop/odr/megatest.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/spreadsheet/crash-no+rows-1.ods";
    const std::string output = "../../test/test.html";
    const std::string password = "password";

    odr::TranslationConfig config = {};
    config.editable = true;
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::OpenDocumentReader translator;
    translator.openOpenDocument(input);
    translator.decrypt(password);
    translator.translate(output, config);
}
