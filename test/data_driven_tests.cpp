#include <access/path.h>
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
      {"passwordEncrypted", meta.passwordEncrypted},
      {"entryCount", meta.documentMeta->entryCount},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.documentMeta->entries.empty()) {
    for (auto &&e : meta.documentMeta->entries) {
      result["entries"].push_back({
          {"name", e.name},
          {"rowCount", e.rowCount},
          {"columnCount", e.columnCount},
          {"notes", e.notes},
      });
    }
  }

  return result;
}
} // namespace

TEST_P(DataDrivenTest, all) {
  const auto testFilePath = GetParam();
  test::TestFile testFile = test::test_meta::instance().testFile(testFilePath);
  const std::string outputPath = "./output/" + testFilePath;

  std::cout << testFile.path << " to " << outputPath << std::endl;

  if ((testFile.type == FileType::ZIP) ||
      (testFile.type == FileType::PORTABLE_DOCUMENT_FORMAT))
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

  // TODO
  // EXPECT_EQ(testFile.encrypted, document.encrypted());
  // if (document.encrypted())
  //  EXPECT_TRUE(document.decrypt(testFile.password));
  EXPECT_EQ(testFile.type, file.fileType());

  fileMeta = file.fileMeta();

  {
    const std::string metaOutput = outputPath + "/meta.json";
    const auto json = metaToJson(file.fileMeta());
    std::ofstream o(metaOutput);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(metaOutput));
    EXPECT_LT(0, fs::file_size(metaOutput));
  }

  // TODO
  // if (!document.translatable())
  //  return;

  if (file.fileCategory() == FileCategory::DOCUMENT) {
    auto document = file.documentFile();
    auto documentMeta = document.documentMeta();

    if (document.documentType() == DocumentType::TEXT) {
      const std::string htmlOutput = outputPath + "/document.html";
      fs::create_directories(fs::path(htmlOutput).parent_path());
      // TODO
      // document.translate(htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    } else if (document.documentType() == DocumentType::PRESENTATION) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/slide" + std::to_string(i) + ".html";
        // TODO
        // document.translate(htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.documentType() == DocumentType::SPREADSHEET) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/sheet" + std::to_string(i) + ".html";
        // TODO
        // document.translate(htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.documentType() == DocumentType::GRAPHICS) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            outputPath + "/page" + std::to_string(i) + ".html";
        // TODO
        // document.translate(htmlOutput, config);
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
    testing::ValuesIn(test::test_meta::instance().testFilePaths()));
