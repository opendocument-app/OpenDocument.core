#include <string>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "ZipFile.h"

namespace opendocument {

TEST(ZipFileTest, open) {
    const std::string path = "../../test/empty.odt";
    auto zip = ZipFile::open(path);
    EXPECT_TRUE(zip);
}

}
