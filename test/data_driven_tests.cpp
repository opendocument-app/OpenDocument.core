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

nlohmann::json metaToJson(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.typeAsString()},
      {"password_encrypted", meta.passwordEncrypted},
      {"entryCount", meta.documentMeta->entry_count},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.documentMeta->entries.empty()) {
    for (auto &&e : meta.documentMeta->entries) {
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
  const auto testFilePath = GetParam();
  test::TestFile testFile = test::TestMeta::instance().testFile(testFilePath);
  const std::string outputPath = "./output/" + testFilePath;

  std::cout << testFile.path << " to " << outputPath << std::endl;

  if ((testFile.type == FileType::ZIP) ||
      (testFile.type == FileType::PORTABLE_DOCUMENT_FORMAT))
    GTEST_SKIP();

  // TODO remove
  if ((testFile.type == FileType::OFFICE_OPEN_XML_DOCUMENT) ||
      (testFile.type == FileType::OFFICE_OPEN_XML_PRESENTATION) ||
      (testFile.type == FileType::OFFICE_OPEN_XML_WORKBOOK) ||
      (testFile.type == FileType::LEGACY_WORD_DOCUMENT) ||
      (testFile.type == FileType::LEGACY_POWERPOINT_PRESENTATION) ||
      (testFile.type == FileType::LEGACY_EXCEL_WORKSHEETS))
    GTEST_SKIP();

  odr::HtmlConfig config;
  config.editable = true;
  config.tableLimitRows = 4000;
  config.tableLimitCols = 500;

  const odr::File file{testFile.path};

  fs::create_directories(fs::path(outputPath));
  auto fileMeta = file.fileMeta();

  // encrypted ooxml type cannot be inspected
  if ((file.fileType() != FileType::OFFICE_OPEN_XML_ENCRYPTED))
    EXPECT_EQ(testFile.type, file.fileType());

  if (file.fileCategory() == FileCategory::DOCUMENT) {
    auto documentFile = file.documentFile();

    EXPECT_EQ(testFile.passwordEncrypted, documentFile.passwordEncrypted());
    if (documentFile.passwordEncrypted())
      EXPECT_TRUE(documentFile.decrypt(testFile.password));
    EXPECT_EQ(testFile.type, documentFile.fileType());
  }

  fileMeta = file.fileMeta();

  {
    const std::string metaOutput = outputPath + "/meta.json";
    const auto json = metaToJson(fileMeta);
    std::ofstream o(metaOutput);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(metaOutput));
    EXPECT_LT(0, fs::file_size(metaOutput));
  }

  if (file.fileCategory() == FileCategory::DOCUMENT) {
    auto documentFile = file.documentFile();
    auto document = documentFile.document();
    auto documentMeta = document.document_meta();

    if (document.document_type() == DocumentType::TEXT) {
      const std::string htmlOutput = outputPath + "/document.html";
      fs::create_directories(fs::path(htmlOutput).parent_path());
      Html::translate(document, "", htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    } else if (document.document_type() == DocumentType::PRESENTATION) {
      for (std::uint32_t i = 0; i < documentMeta.entry_count; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/slide" + std::to_string(i) + ".html";
        Html::translate(document, "", htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.document_type() == DocumentType::SPREADSHEET) {
      for (std::uint32_t i = 0; i < documentMeta.entry_count; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/sheet" + std::to_string(i) + ".html";
        // TODO
        // Html::translate(document, "", htmlOutput, config);
        // EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        // EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.document_type() == DocumentType::DRAWING) {
      for (std::uint32_t i = 0; i < documentMeta.entry_count; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/page" + std::to_string(i) + ".html";
        Html::translate(document, "", htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else {
      EXPECT_TRUE(false);
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    all, DataDrivenTest,
    testing::ValuesIn(test::TestMeta::instance().testFilePaths()));
