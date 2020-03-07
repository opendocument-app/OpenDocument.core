#include "ooxml/OfficeOpenXmlMeta.h"
#include "glog/logging.h"
#include "io/ZipStorage.h"
#include "odr/Meta.h"
#include "gtest/gtest.h"
#include <string>

TEST(OfficeOpenXmlMeta, open) {
    const std::string path = "../../test/empty.docx";

    odr::ZipReader ooxml(path);
    const odr::FileMeta meta = odr::OfficeOpenXmlMeta::parseFileMeta(ooxml);

    LOG(INFO) << (int) meta.type;
    LOG(INFO) << meta.entryCount;

    EXPECT_NE(meta.type, odr::FileType::UNKNOWN);
    //EXPECT_GE(meta.entryCount, 0);
}
