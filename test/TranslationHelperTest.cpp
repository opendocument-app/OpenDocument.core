#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

namespace odr {

TEST(TranslationHelperTest, translate) {
    const std::string input = "/home/andreas/Downloads/03_smpldap.odt";
    const std::string output = "../../test/test.html";

    TranslationConfig config = {};
    auto translator = TranslationHelper::create(config);
    translator->translate(input, output);
}

}
