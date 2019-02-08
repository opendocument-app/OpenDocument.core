#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"
#include "TextTranslator.h"

namespace opendocument {

TEST(TextTranslatorTest, translate) {
    const std::string input = "/home/andreas/Downloads/SSH_Recherche.odt";
    const std::string output = "../../test/test.html";

    OpenDocumentFile odf(input);

    TextTranslator translator;
    translator.translate(odf, output);
}

}
