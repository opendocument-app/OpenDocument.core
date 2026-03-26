#include <gtest/gtest.h>

#include <test_util.hpp>

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  odr::test::set_global_params();

  return RUN_ALL_TESTS();
}
