#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <test_util.hpp>

#include <odr/html.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
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
  const std::string output_path_prefix =
      common::Path("output").join(test_repo).join("output-pdf2htmlEX").string();
  const std::string output_path =
      common::Path(output_path_prefix)
          .join(common::Path(test_file_path).rebase(test_repo))
          .string();

  std::cout << test_file.path << " to " << output_path << std::endl;

  fs::create_directories(output_path);
  HtmlConfig config;

  PopplerPdfFile pdf_file(std::make_shared<common::DiskFile>(test_file.path));

  EXPECT_EQ(test_file.password_encrypted, pdf_file.password_encrypted());
  if (test_file.password_encrypted) {
    EXPECT_TRUE(pdf_file.decrypt(test_file.password));
    EXPECT_EQ(EncryptionState::decrypted, pdf_file.encryption_state());
  }

  Html html = odr::internal::html::translate_poppler_pdf_file(
      pdf_file, output_path, config);
  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

INSTANTIATE_TEST_SUITE_P(pdf2htmlEX_test_files, pdf2htmlEXWrapperTests,
                         testing::ValuesIn(TestData::test_file_paths(
                             FileType::portable_document_format)),
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
