#include <odr/html.hpp>
#include <odr/open_document_reader.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/util/odr_meta_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <test_util.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

using HtmlOutputTests = ::testing::TestWithParam<std::string>;

TEST_P(HtmlOutputTests, html_meta) {
  const std::string test_file_path = GetParam();
  const TestFile test_file = TestData::test_file(test_file_path);

  const std::string test_repo = *common::Path(test_file_path).begin();
  const std::string output_path_prefix =
      common::Path("output").join(test_repo).join("output").string();
  const std::string output_path =
      common::Path(output_path_prefix)
          .join(common::Path(test_file_path).rebase(test_repo))
          .string();

  std::cout << test_file.path << " to " << output_path << std::endl;

  // TODO compare guessed file type VS actual file type

  // these files cannot be opened
  if (util::string::ends_with(test_file.path, ".sxw") ||
      (test_file.type == FileType::legacy_word_document) ||
      (test_file.type == FileType::legacy_powerpoint_presentation) ||
      (test_file.type == FileType::legacy_excel_worksheets) ||
      (test_file.type == FileType::word_perfect) ||
      (test_file.type == FileType::starview_metafile)) {
    GTEST_SKIP();
  }

  const DecodedFile file{test_file.path};

  FileMeta file_meta = file.file_meta();

  // encrypted ooxml type cannot be inspected
  if ((file.file_type() != FileType::office_open_xml_encrypted)) {
    EXPECT_EQ(test_file.type, file.file_type());
  }

  // TODO enable zip, csv, json
  if ((test_file.type == FileType::zip) ||
      (test_file.type == FileType::comma_separated_values) ||
      (test_file.type == FileType::javascript_object_notation)) {
    GTEST_SKIP();
  }

  if (file.is_document_file()) {
    DocumentFile document_file = file.document_file();

    EXPECT_EQ(test_file.password_encrypted, document_file.password_encrypted());
    if (document_file.password_encrypted()) {
      EXPECT_TRUE(document_file.decrypt(test_file.password));
    }
    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  fs::create_directories(output_path);
  file_meta = file.file_meta();

  {
    const std::string meta_output = output_path + "/meta.json";
    const nlohmann::json json =
        odr::internal::util::meta::meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
  }

  const std::string resource_path =
      common::Path(output_path_prefix).parent().join("resources").string();
  OpenDocumentReader::copy_resources(resource_path);

  HtmlConfig config;
  config.embed_resources = false;
  config.external_resource_path = resource_path;
  config.relative_resource_paths = true;
  config.editable = true;
  config.spreadsheet_limit = TableDimensions(4000, 500);
  config.format_html = true;
  config.html_indent = 2;

  Html html = OpenDocumentReader::html(file, output_path, config);

  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

INSTANTIATE_TEST_SUITE_P(all_test_files, HtmlOutputTests,
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
