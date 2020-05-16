#include <common/CssWriter.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace odr::common;

TEST(CssWriter, foo) {
  std::stringstream output;

  {
    CssWriter css{output};
    css.selector("*").property("hello", "world");
  }

  const std::string result = output.str();
  EXPECT_EQ("*{hello:world}", result);
}
