#include <common/HtmlWriter.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace odr::common;

TEST(HtmlWriter, foo) {
  std::stringstream output;

  {
    HtmlWriter html{output};

    {
      auto head = html.head("hello world");
      head.meta("a", "b");
    }

    {
      auto body = html.body();
      body.text("success");
    }
  }

  const std::string result = output.str();
  std::cout << result;
}
