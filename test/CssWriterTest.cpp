#include <common/CssWriter.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace odr::common;

TEST(CssWriter, foo) {
  std::unique_ptr<std::ostream> output = std::make_unique<std::stringstream>();

  {
    CssWriter css{std::move(output), output};
    css.selector("*").declaration("hello", "world");
  }

  const std::string result = dynamic_cast<std::stringstream *>(output.get())->str();
  EXPECT_EQ("*{hello:world}", result);
}
