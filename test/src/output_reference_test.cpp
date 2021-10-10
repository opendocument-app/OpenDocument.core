#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <internal/common/path.h>
#include <internal/util/odr_meta_util.h>
#include <internal/util/string_util.h>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/file.h>
#include <odr/html.h>
#include <odr/style.h>
#include <optional>
#include <string>
#include <test_util.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

class OutputReferenceTests : public testing::TestWithParam<std::string> {};

TEST_P(OutputReferenceTests, all) {
  const auto test_file_path = GetParam();
  TestFile test_file = TestData::test_file(test_file_path);
  const std::string output_path = "./output/" + test_file_path;

  std::cout << test_file.path << " to " << output_path << std::endl;

  if ((test_file.type == FileType::zip) ||
      (test_file.type == FileType::portable_document_format)) {
    GTEST_SKIP();
  }

  // TODO remove
  if (util::string::ends_with(test_file.path, ".sxw") ||
      (test_file.type == FileType::legacy_word_document) ||
      (test_file.type == FileType::legacy_powerpoint_presentation) ||
      (test_file.type == FileType::legacy_excel_worksheets) ||
      (test_file.type == FileType::starview_metafile)) {
    GTEST_SKIP();
  }

  HtmlConfig config;
  config.editable = true;
  config.table_limit = TableDimensions(4000, 500);

  const DecodedFile file{test_file.path};

  auto file_meta = file.file_meta();

  // encrypted ooxml type cannot be inspected
  if ((file.file_type() != FileType::office_open_xml_encrypted)) {
    EXPECT_EQ(test_file.type, file.file_type());
  }

  if (file.file_category() == FileCategory::document) {
    auto document_file = file.document_file();

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
    const auto json = odr::internal::util::meta::meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
  }

  if (file.file_category() == FileCategory::document) {
    auto document_file = file.document_file();
    auto document = document_file.document();
    auto cursor = document.root_element();

    if (document.document_type() == DocumentType::text) {
      const std::string html_output = output_path + "/document.html";
      fs::create_directories(fs::path(html_output).parent_path());
      html::translate(document, html_output, config);
      EXPECT_TRUE(fs::is_regular_file(html_output));
      EXPECT_LT(0, fs::file_size(html_output));
    } else if (document.document_type() == DocumentType::presentation) {
      cursor.for_each_child([&](DocumentCursor &, const std::uint32_t i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/slide" + std::to_string(i) + ".html";
        html::translate(document, html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      });
    } else if (document.document_type() == DocumentType::spreadsheet) {
      cursor.for_each_child([&](DocumentCursor &, const std::uint32_t i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/sheet" + std::to_string(i) + ".html";
        html::translate(document, html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      });
    } else if (document.document_type() == DocumentType::drawing) {
      cursor.for_each_child([&](DocumentCursor &, const std::uint32_t i) {
        config.entry_offset = i;
        config.entry_count = 1;
        const std::string html_output =
            output_path + "/page" + std::to_string(i) + ".html";
        html::translate(document, html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      });
    } else {
      EXPECT_TRUE(false);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(all, OutputReferenceTests,
                         testing::ValuesIn(TestData::test_file_paths()));
