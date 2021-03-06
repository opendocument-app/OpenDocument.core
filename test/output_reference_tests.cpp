#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <internal/common/path.h>
#include <internal/util/odr_meta_util.h>
#include <internal/util/string_util.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/file_category.h>
#include <odr/file_meta.h>
#include <odr/html.h>
#include <test/test_util.h>
#include <utility>

using namespace odr;
using namespace odr::test;
using namespace odr::internal;
namespace fs = std::filesystem;

class OutputReferenceTests : public testing::TestWithParam<std::string> {};

TEST_P(OutputReferenceTests, all) {
  const auto test_file_path = GetParam();
  TestFile test_file = TestData::test_file(test_file_path);
  const std::string output_path = "./output/" + test_file_path;

  std::cout << test_file.path << " to " << output_path << std::endl;

  if ((test_file.type == FileType::ZIP) ||
      (test_file.type == FileType::PORTABLE_DOCUMENT_FORMAT)) {
    GTEST_SKIP();
  }

  // TODO remove
  if (util::string::ends_with(test_file.path, ".sxw") ||
      (test_file.type == FileType::LEGACY_WORD_DOCUMENT) ||
      (test_file.type == FileType::LEGACY_POWERPOINT_PRESENTATION) ||
      (test_file.type == FileType::LEGACY_EXCEL_WORKSHEETS)) {
    GTEST_SKIP();
  }

  odr::HtmlConfig config;
  config.editable = true;
  config.table_limit_rows = 4000;
  config.table_limit_cols = 500;

  const odr::DecodedFile file{test_file.path};

  fs::create_directories(fs::path(output_path));
  auto file_meta = file.file_meta();

  // encrypted ooxml type cannot be inspected
  if ((file.file_type() != FileType::OFFICE_OPEN_XML_ENCRYPTED)) {
    EXPECT_EQ(test_file.type, file.file_type());
  }

  if (file.file_category() == FileCategory::DOCUMENT) {
    auto document_file = file.document_file();

    EXPECT_EQ(test_file.password_encrypted, document_file.password_encrypted());
    if (document_file.password_encrypted()) {
      EXPECT_TRUE(document_file.decrypt(test_file.password));
    }
    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  file_meta = file.file_meta();

  {
    const std::string meta_output = output_path + "/meta.json";
    const auto json = odr::internal::util::meta::meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
  }

  // TODO remove
  if ((test_file.type != FileType::OPENDOCUMENT_TEXT) &&
      (test_file.type != FileType::OPENDOCUMENT_PRESENTATION) &&
      (test_file.type != FileType::OPENDOCUMENT_SPREADSHEET) &&
      (test_file.type != FileType::OPENDOCUMENT_GRAPHICS) &&
      (test_file.type != FileType::OFFICE_OPEN_XML_DOCUMENT)) {
    GTEST_SKIP();
  }

  if (file.file_category() == FileCategory::DOCUMENT) {
    auto document_file = file.document_file();
    auto document = document_file.document();
    auto document_meta = document.document_meta();

    if (document.document_type() == DocumentType::TEXT) {
      const std::string html_output = output_path + "/document.html";
      fs::create_directories(fs::path(html_output).parent_path());
      Html::translate(document, "", html_output, config);
      EXPECT_TRUE(fs::is_regular_file(html_output));
      EXPECT_LT(0, fs::file_size(html_output));
    } else if (document.document_type() == DocumentType::PRESENTATION) {
      for (std::uint32_t i = 0; i < document_meta.entry_count; ++i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/slide" + std::to_string(i) + ".html";
        Html::translate(document, "", html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      }
    } else if (document.document_type() == DocumentType::SPREADSHEET) {
      for (std::uint32_t i = 0; i < document_meta.entry_count; ++i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/sheet" + std::to_string(i) + ".html";
        Html::translate(document, "", html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      }
    } else if (document.document_type() == DocumentType::DRAWING) {
      for (std::uint32_t i = 0; i < document_meta.entry_count; ++i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/page" + std::to_string(i) + ".html";
        Html::translate(document, "", html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      }
    } else {
      EXPECT_TRUE(false);
    }
  }
}

INSTANTIATE_TEST_CASE_P(all, OutputReferenceTests,
                        testing::ValuesIn(TestData::test_file_paths()));
