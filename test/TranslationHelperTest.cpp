#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "opendocument/TranslationHelper.h"

namespace opendocument {

TEST(TranslationHelperTest, translate) {
    const std::string input = "/home/andreas/Downloads/03_smpldap.odt";
    const std::string output = "../../test/test.html";

    TranslationHelper::instance().translate(input, output);
}

}
