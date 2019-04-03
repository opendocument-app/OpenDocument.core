#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

namespace odr {

TEST(TranslationHelperTest, translate) {
    std::string input;
    input = "/home/andreas/workspace/OpenDocument.test/files/spreadsheet/efficiency-big-2.ods";
    input = "/home/andreas/Desktop/odr/test.odp";
    const std::string output = "../../test/test.html";

    TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;
    auto translator = TranslationHelper::create();
    translator->open(input);
    translator->translate(output, config);
}

}
