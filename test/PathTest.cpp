#include <unordered_map>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "io/Path.h"

namespace odr {

TEST(Path, empty) {
    EXPECT_EQ("", Path().string());
}

TEST(Path, join) {
    Path a("ppt/slides");
    Path b("../media/image8.png");

    EXPECT_EQ("ppt/media/image8.png", a.join(b).string());
}

}
