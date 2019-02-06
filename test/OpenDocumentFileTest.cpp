#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "ZipFile.h"
#include "OpenDocumentFile.h"

namespace opendocument {

TEST(OpenDocumentFileTest, open) {
    const std::string path = "../../test/empty.odt";
    auto zip = ZipFile::open(path);
    OpenDocumentFile odf(*zip);
    LOG(INFO) << odf.getSize("mimetype");
}

}
