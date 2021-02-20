#include <common/path.h>
#include <csv.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <map>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/html.h>
#include <test/test_meta.h>
#include <utility>

using namespace odr;
namespace fs = std::filesystem;

namespace {
class DataDrivenTest : public testing::TestWithParam<std::string> {};

nlohmann::json meta_to_json(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.type_as_string()},
      {"password_encrypted", meta.password_encrypted},
      {"entryCount", meta.document_meta->entry_count},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.document_meta->entries.empty()) {
    for (auto &&e : meta.document_meta->entries) {
      result["entries"].push_back({
          {"name", e.name},
          {"rowCount", e.row_count},
          {"columnCount", e.column_count},
          {"notes", e.notes},
      });
    }
  }

  return result;
}
} // namespace

TEST_P(DataDrivenTest, all) {
  const auto test_file_path = GetParam();
  test::TestFile test_file =
      test::TestMeta::instance().test_file(test_file_path);
  const std::string output_path = "./output/" + test_file_path;

  std::cout << test_file.path << " to " << output_path << std::endl;

  if ((test_file.type == FileType::ZIP) ||
      (test_file.type == FileType::PORTABLE_DOCUMENT_FORMAT)) {
    GTEST_SKIP();
  }

  // TODO remove
  if ((test_file.type == FileType::OFFICE_OPEN_XML_DOCUMENT) ||
      (test_file.type == FileType::OFFICE_OPEN_XML_PRESENTATION) ||
      (test_file.type == FileType::OFFICE_OPEN_XML_WORKBOOK) ||
      (test_file.type == FileType::LEGACY_WORD_DOCUMENT) ||
      (test_file.type == FileType::LEGACY_POWERPOINT_PRESENTATION) ||
      (test_file.type == FileType::LEGACY_EXCEL_WORKSHEETS)) {
    GTEST_SKIP();
  }

  odr::HtmlConfig config;
  config.editable = true;
  config.tableLimitRows = 4000;
  config.tableLimitCols = 500;

  //const odr::File file{test_file.path};
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
    if (document_file.password_encrypted())
      EXPECT_TRUE(document_file.decrypt(test_file.password));
    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  file_meta = file.file_meta();

  {
    const std::string meta_output = output_path + "/meta.json";
    const auto json = meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
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
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string html_output =
            output_path + "/slide" + std::to_string(i) + ".html";
        Html::translate(document, "", html_output, config);
        EXPECT_TRUE(fs::is_regular_file(html_output));
        EXPECT_LT(0, fs::file_size(html_output));
      }
    } else if (document.document_type() == DocumentType::SPREADSHEET) {
      for (std::uint32_t i = 0; i < document_meta.entry_count; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string html_output =
            output_path + "/sheet" + std::to_string(i) + ".html";
        // TODO
        // Html::translate(document, "", htmlOutput, config);
        // EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        // EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.document_type() == DocumentType::DRAWING) {
      for (std::uint32_t i = 0; i < document_meta.entry_count; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
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

INSTANTIATE_TEST_CASE_P(
    all, DataDrivenTest,
    testing::ValuesIn(test::TestMeta::instance().test_file_paths()));
