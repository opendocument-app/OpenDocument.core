#include <test_util.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <exception>
#include <sstream>
#include <string_view>

using namespace odr;
using namespace odr::test;

TEST(MalformedCorpus, every_file_fails_with_its_error) {
  const auto files = TestData::malformed_test_files();
  if (files.empty()) {
    GTEST_SKIP() << "No malformed corpus files available";
  }

  for (const TestFile &test_file : files) {
    SCOPED_TRACE(test_file.short_path);
    try {
      const DecodedFile file =
          open(test_file.absolute_path, DecodeOptions::as(test_file.type));
      for (const HtmlView &view :
           html::translate(file, HtmlConfig()).list_views()) {
        std::ostringstream out;
        view.write_html(out);
      }
      ADD_FAILURE() << "rendered without an error";
    } catch (const std::exception &e) {
      EXPECT_EQ(std::string_view(e.what()), *test_file.error);
    }
  }
}
