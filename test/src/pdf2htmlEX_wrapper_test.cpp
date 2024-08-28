#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <test_util.hpp>

#include <odr/html.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>
#include <odr/internal/util/string_util.hpp>

using namespace odr;
using namespace odr::test;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

using pdf2htmlEXWrapperTests = ::testing::TestWithParam<std::string>;

TEST_P(pdf2htmlEXWrapperTests, html) {
  const std::string test_file_path = GetParam();
  const TestFile test_file = TestData::test_file(test_file_path);

  const std::string test_repo = *common::Path(test_file_path).begin();
  const std::string output_path_prefix = common::Path("output")
                                             .join(test_repo)
                                             .join("output")
                                             .join("pdf2htmlEX")
                                             .string();
  const std::string output_path =
      common::Path(output_path_prefix)
          .join(common::Path(test_file_path).rebase(test_repo))
          .string();

  std::cout << test_file.path << " to " << output_path << std::endl;

  if (!util::string::ends_with(test_file.path, ".pdf") &&
      test_file.type != FileType::portable_document_format) {
    GTEST_SKIP();
  }

  fs::create_directories(output_path);
  HtmlConfig config;
  std::optional<std::string> password;

  if (test_file.password_encrypted) {
    password = test_file.password;
  }

  Html html = odr::internal::html::pdf2htmlEX_wrapper(
      test_file.path, output_path, config, password);

  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

INSTANTIATE_TEST_SUITE_P(all_test_files, pdf2htmlEXWrapperTests,
                         testing::ValuesIn(TestData::test_file_paths()),
                         [](const ::testing::TestParamInfo<std::string> &info) {
                           std::string path = info.param;
                           internal::util::string::replace_all(path, "/", "_");
                           internal::util::string::replace_all(path, "-", "_");
                           internal::util::string::replace_all(path, "+", "_");
                           internal::util::string::replace_all(path, ".", "_");
                           internal::util::string::replace_all(path, " ", "_");
                           internal::util::string::replace_all(path, "$", "");
                           return path;
                         });
