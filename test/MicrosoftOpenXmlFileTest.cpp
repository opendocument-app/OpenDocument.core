#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "MicrosoftOpenXmlFile.h"

TEST(MicrosoftOpenXmlFileTest, open) {
    const std::string path = "../../test/empty.docx";

    odr::MicrosoftOpenXmlFile docx;
    docx.open(path);

    LOG(INFO) << (int) docx.getMeta().type;
    LOG(INFO) << docx.getMeta().entryCount;
}
