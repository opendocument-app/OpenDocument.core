#include "gtest/gtest.h"
#include "glog/logging.h"
#include "TableLocation.h"

namespace odr {

TEST(TableLocation, test) {
    TableLocation tl;
    EXPECT_EQ(tl.getNextRow(), 0);
    EXPECT_EQ(tl.getNextCol(), 0);
    tl.addRow(1);
    EXPECT_EQ(tl.getNextRow(), 1);
    EXPECT_EQ(tl.getNextCol(), 0);
    tl.addCell(2, 2, 1);
    EXPECT_EQ(tl.getNextRow(), 1);
    EXPECT_EQ(tl.getNextCol(), 2);
    tl.addCell(2, 1, 1);
    EXPECT_EQ(tl.getNextRow(), 1);
    EXPECT_EQ(tl.getNextCol(), 4);
    tl.addRow(1);
    EXPECT_EQ(tl.getNextRow(), 2);
    EXPECT_EQ(tl.getNextCol(), 2);
    tl.addCell(1, 1, 1);
    tl.addCell(1, 1, 1);
    EXPECT_EQ(tl.getNextRow(), 2);
    EXPECT_EQ(tl.getNextCol(), 4);
    tl.addRow(1);
    EXPECT_EQ(tl.getNextRow(), 3);
    EXPECT_EQ(tl.getNextCol(), 0);
}

}
