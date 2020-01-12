#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "odr/FileMeta.h"
#include "io/ZipStorage.h"
#include "odf/OpenDocumentMeta.h"

TEST(OpenDocumentMetaTest, open) {
    const std::string path = "../../test/empty.odp";

    odr::ZipReader odf(path);
    const odr::FileMeta meta = odr::OpenDocumentMeta::parseFileMeta(odf, false);

    LOG(INFO) << (int) meta.type;
    LOG(INFO) << meta.entryCount;

    EXPECT_NE(meta.type, odr::FileType::UNKNOWN);
    EXPECT_GE(meta.entryCount, 0);
}
