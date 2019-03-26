#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"

namespace odr {

TEST(OpenDocumentFileTest, open) {
    const std::string path = "../../test/empty.ods";
    auto odf = OpenDocumentFile::create();
    odf->open(path);

    LOG(INFO) << (int) odf->getMeta().type;
    LOG(INFO) << odf->getMeta().text.pageCount;
    LOG(INFO) << odf->getMeta().spreadsheet.tableCount;
}

}
