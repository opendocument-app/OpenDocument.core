#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <test_util.hpp>

#include <odr/html.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/wvWare_wrapper.hpp>
#include <odr/internal/util/string_util.hpp>

using namespace odr;
using namespace odr::test;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

using wvWareWrapperTests = ::testing::TestWithParam<std::string>;

TEST_P(wvWareWrapperTests, html) {
  const std::string test_file_path = GetParam();
  const TestFile test_file = TestData::test_file(test_file_path);

  const std::string test_repo = *common::Path(test_file_path).begin();
  const std::string output_path_prefix = common::Path("output")
                                             .join(test_repo)
                                             .join("output-wvWare")
                                             .string();
  const std::string output_path =
      common::Path(output_path_prefix)
          .join(common::Path(test_file_path).rebase(test_repo))
          .string();

  std::cout << test_file.path << " to " << output_path << std::endl;

  // Password protected files are problematic on wvWare
  if (test_file.password_encrypted) {
    GTEST_SKIP();
  }

  fs::create_directories(output_path);
  HtmlConfig config;
  std::optional<std::string> password;
  Html html = odr::internal::html::wvWare_wrapper(test_file.path, output_path,
                                                  config, password);

  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

INSTANTIATE_TEST_SUITE_P(wvWare_test_files, wvWareWrapperTests,
                         testing::ValuesIn(TestData::test_file_paths(
                             FileType::legacy_word_document)),
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
