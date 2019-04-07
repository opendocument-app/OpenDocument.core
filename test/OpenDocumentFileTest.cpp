#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"

TEST(OpenDocumentFileTest, open) {
    const std::string path = "../../test/empty.odp";
    auto odf = odr::OpenDocumentFile::create();
    odf->open(path);

    LOG(INFO) << (int) odf->getMeta().type;
    LOG(INFO) << odf->getMeta().entryCount;
}
