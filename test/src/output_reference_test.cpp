#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <internal/common/path.h>
#include <internal/util/odr_meta_util.h>
#include <internal/util/string_util.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/file_meta.h>
#include <odr/html_config.h>
#include <test_util.h>
#include <utility>

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

  if ((test_file.type == FileType::ZIP) ||
      (test_file.type == FileType::PORTABLE_DOCUMENT_FORMAT)) {
    GTEST_SKIP();
  }

  HtmlConfig config;
  config.editable = true;
  config.table_limit_rows = 4000;
  config.table_limit_cols = 500;

  const Document document{test_file.path};

  fs::create_directories(fs::path(output_path));
  auto meta = document.meta();

  // encrypted ooxml type cannot be inspected
  if (document.type() != FileType::OFFICE_OPEN_XML_ENCRYPTED) {
    EXPECT_EQ(test_file.type, document.type());
  }

  if ((test_file.type == FileType::LEGACY_WORD_DOCUMENT) ||
      (test_file.type == FileType::LEGACY_POWERPOINT_PRESENTATION) ||
      (test_file.type == FileType::LEGACY_EXCEL_WORKSHEETS)) {
    GTEST_SKIP();
  }

  EXPECT_EQ(test_file.password_encrypted, document.encrypted());
  if (document.encrypted()) {
    EXPECT_TRUE(document.decrypt(test_file.password));
  }
  EXPECT_EQ(test_file.type, document.type());

  meta = document.meta();

  {
    const std::string meta_output = output_path + "/meta.json";
    const auto json = internal::util::meta::meta_to_json(meta);
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

  if (!document.translatable()) {
    return;
  }

  if ((meta.type == FileType::OPENDOCUMENT_TEXT) ||
      (meta.type == FileType::OFFICE_OPEN_XML_DOCUMENT)) {
    const std::string html_output = output_path + "/document.html";
    fs::create_directories(fs::path(html_output).parent_path());
    document.translate(html_output, config);
    EXPECT_TRUE(fs::is_regular_file(html_output));
    EXPECT_LT(0, fs::file_size(html_output));
  } else if ((meta.type == FileType::OPENDOCUMENT_PRESENTATION) ||
             (meta.type == FileType::OFFICE_OPEN_XML_PRESENTATION)) {
    for (std::uint32_t i = 0; i < meta.entry_count; ++i) {
      config.entry_offset = i;
      config.entry_count = 1;
      const std::string html_output =
          output_path + "/slide" + std::to_string(i) + ".html";
      document.translate(html_output, config);
      EXPECT_TRUE(fs::is_regular_file(html_output));
      EXPECT_LT(0, fs::file_size(html_output));
    }
  } else if ((meta.type == FileType::OPENDOCUMENT_SPREADSHEET) ||
             (meta.type == FileType::OFFICE_OPEN_XML_WORKBOOK)) {
    for (std::uint32_t i = 0; i < meta.entry_count; ++i) {
      config.entry_offset = i;
      config.entry_count = 1;
      const std::string html_output =
          output_path + "/sheet" + std::to_string(i) + ".html";
      document.translate(html_output, config);
      EXPECT_TRUE(fs::is_regular_file(html_output));
      EXPECT_LT(0, fs::file_size(html_output));
    }
  } else if (meta.type == FileType::OPENDOCUMENT_GRAPHICS) {
    for (std::uint32_t i = 0; i < meta.entry_count; ++i) {
      config.entry_offset = i;
      config.entry_count = 1;
      const std::string html_output =
          output_path + "/page" + std::to_string(i) + ".html";
      document.translate(html_output, config);
      EXPECT_TRUE(fs::is_regular_file(html_output));
      EXPECT_LT(0, fs::file_size(html_output));
    }
  } else {
    EXPECT_TRUE(false);
  }
}

INSTANTIATE_TEST_CASE_P(all, OutputReferenceTests,
                        testing::ValuesIn(TestData::test_file_paths()));
