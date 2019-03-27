#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

namespace odr {

TEST(TranslationHelperTest, translate) {
    std::string input;
    input = "/home/andreas/Downloads/03_smpldap.odt";
    input = "/home/andreas/workspace/OpenDocument.test/files/spreadsheet/efficiency-big-2.ods";
    const std::string output = "../../test/test.html";

    TranslationConfig config = {};
    auto translator = TranslationHelper::create(config);
    translator->translate(input, output);
}

}
