#include <access/ZipStorage.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <odf/OpenDocumentMeta.h>
#include <odr/Meta.h>
#include <string>

TEST(OpenDocumentMeta, open) {
  const std::string path = "../../test/empty.odp";

  odr::access::ZipReader odf(path);
  const odr::FileMeta meta =
      odr::odf::OpenDocumentMeta::parseFileMeta(odf, false);

  LOG(INFO) << (int)meta.type;
  LOG(INFO) << meta.entryCount;

  EXPECT_NE(meta.type, odr::FileType::UNKNOWN);
  EXPECT_GE(meta.entryCount, 0);
}
