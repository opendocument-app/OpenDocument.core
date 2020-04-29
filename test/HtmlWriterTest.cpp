#include <common/HtmlWriter.h>
#include <gtest/gtest.h>
#include <sstream>

using namespace odr::common;

TEST(HtmlWriter, foo) {
  std::unique_ptr<std::ostream> output = std::make_unique<std::stringstream>();

  {
    HtmlWriter html{std::move(output), output};
    auto head = html.head("hello world");
    head.meta("a", "b");
    auto body = head.body();
    body.text("success");
  }

  const std::string result = dynamic_cast<std::stringstream *>(output.get())->str();
  std::cout << result;
}
