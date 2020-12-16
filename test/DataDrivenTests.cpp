#include <access/Path.h>
#include <csv.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <map>
#include <nlohmann/json.hpp>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Meta.h>
#include <test/TestMeta.h>
#include <utility>

using namespace odr;
namespace fs = std::filesystem;

namespace {
class DataDrivenTest : public testing::TestWithParam<std::string> {};

nlohmann::json metaToJson(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.typeAsString()},
      {"encrypted", meta.encrypted},
      {"entryCount", meta.entryCount},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.entries.empty()) {
    for (auto &&e : meta.entries) {
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
  const auto name = GetParam();
  test::TestFile testFile = test::TestMeta::instance().testFile(name);
  const std::string outputPath = "./output/" + name;

  std::cout << testFile.path << " to " << outputPath << std::endl;

  if ((testFile.type == FileType::ZIP) ||
      (testFile.type == FileType::PORTABLE_DOCUMENT_FORMAT))
    GTEST_SKIP();

  odr::Config config;
  config.editable = true;
  config.tableLimitRows = 4000;
  config.tableLimitCols = 500;

  const odr::Document document{testFile.path};

  fs::create_directories(fs::path(outputPath));
  auto meta = document.meta();

  // encrypted ooxml type cannot be inspected
  if ((document.type() != FileType::OFFICE_OPEN_XML_ENCRYPTED))
    EXPECT_EQ(testFile.type, document.type());
  if (!meta.confident)
    return;

  EXPECT_EQ(testFile.encrypted, document.encrypted());
  if (document.encrypted())
    EXPECT_TRUE(document.decrypt(testFile.password));
  EXPECT_EQ(testFile.type, document.type());

  meta = document.meta();

  {
    const std::string metaOutput = outputPath + "/meta.json";
    const auto json = metaToJson(meta);
    std::ofstream o(metaOutput);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(metaOutput));
    EXPECT_LT(0, fs::file_size(metaOutput));
  }

  if (!document.translatable())
    return;

  if ((meta.type == FileType::OPENDOCUMENT_TEXT) ||
      (meta.type == FileType::OFFICE_OPEN_XML_DOCUMENT)) {
    const std::string htmlOutput = outputPath + "/document.html";
    fs::create_directories(fs::path(htmlOutput).parent_path());
    document.translate(htmlOutput, config);
    EXPECT_TRUE(fs::is_regular_file(htmlOutput));
    EXPECT_LT(0, fs::file_size(htmlOutput));
  } else if ((meta.type == FileType::OPENDOCUMENT_PRESENTATION) ||
             (meta.type == FileType::OFFICE_OPEN_XML_PRESENTATION)) {
    for (std::uint32_t i = 0; i < meta.entryCount; ++i) {
      config.entryOffset = i;
      config.entryCount = 1;
      const std::string htmlOutput =
          outputPath + "/slide" + std::to_string(i) + ".html";
      document.translate(htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    }
  } else if ((meta.type == FileType::OPENDOCUMENT_SPREADSHEET) ||
             (meta.type == FileType::OFFICE_OPEN_XML_WORKBOOK)) {
    for (std::uint32_t i = 0; i < meta.entryCount; ++i) {
      config.entryOffset = i;
      config.entryCount = 1;
      const std::string htmlOutput =
          outputPath + "/sheet" + std::to_string(i) + ".html";
      document.translate(htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    }
  } else if (meta.type == FileType::OPENDOCUMENT_GRAPHICS) {
    for (std::uint32_t i = 0; i < meta.entryCount; ++i) {
      config.entryOffset = i;
      config.entryCount = 1;
      const std::string htmlOutput =
          outputPath + "/page" + std::to_string(i) + ".html";
      document.translate(htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    }
  } else {
    EXPECT_TRUE(false);
  }
}

INSTANTIATE_TEST_CASE_P(
    all, DataDrivenTest,
    testing::ValuesIn(test::TestMeta::instance().testFiles()));
