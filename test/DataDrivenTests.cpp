#include <access/Path.h>
#include <csv.hpp>
#include <filesystem>
#include <gtest/gtest.h>
#include <map>
#include <nlohmann/json.hpp>
#include <odr/Document.h>
#include <odr/File.h>
#include <odr/Html.h>
#include <utility>

using namespace odr;
namespace fs = std::filesystem;

namespace {
struct Param {
  std::string input;
  FileType type{FileType::UNKNOWN};
  bool encrypted{false};
  std::string password;
  std::string output;

  Param(std::string input, const FileType type, const bool encrypted,
        std::string password, std::string output)
      : input{std::move(input)}, type{type}, encrypted{encrypted},
        password{std::move(password)}, output{std::move(output)} {}
};

using Params = std::vector<Param>;

class DataDrivenTest : public testing::TestWithParam<Param> {};

Param getTestParam(std::string input, std::string output) {
  const FileType type =
      FileMeta::typeByExtension(access::Path(input).extension());
  const std::string fileName = fs::path(input).filename();
  std::string password;
  if (const auto left = fileName.find('$'), right = fileName.rfind('$');
      (left != std::string::npos) && (left != right))
    password = fileName.substr(left, right);
  const bool encrypted = !password.empty();
  output += "/" + fileName;

  return {std::move(input), type, encrypted, std::move(password),
          std::move(output)};
}

Params getTestParams(const std::string &input, std::string output) {
  if (fs::is_regular_file(input))
    return {getTestParam(input, std::move(output))};
  if (!fs::is_directory(input))
    return {};

  Params result;

  if (const std::string index = input + "/index.csv";
      fs::is_regular_file(index)) {
    for (auto &&row : csv::CSVReader(index)) {
      const std::string path = input + "/" + row["path"].get<>();
      const FileType type = FileMeta::typeByExtension(row["type"].get<>());
      std::string password = row["password"].get<>();
      const bool encrypted = !password.empty();
      const std::string fileName = fs::path(path).filename();
      std::string outputTmp =
          output + "/" + access::Path(row["path"].get<>()).parent().string() +
          "/" + fileName;

      if (type == FileType::UNKNOWN)
        continue;
      result.emplace_back(path, type, encrypted, std::move(password),
                          std::move(outputTmp));
    }
  }

  // TODO this will also recurse `.git`
  for (auto &&p : fs::recursive_directory_iterator(input)) {
    if (!p.is_regular_file())
      continue;
    const std::string path = p.path().string();
    if (const auto it =
            std::find_if(std::begin(result), std::end(result),
                         [&](auto &&param) { return param.input == path; });
        it != std::end(result))
      continue;

    std::string outputTmp =
        output + "/" + access::Path(path).rebase(input).parent().string();
    const auto param = getTestParam(path, std::move(outputTmp));

    if (param.type == FileType::UNKNOWN)
      continue;
    result.push_back(param);
  }

  return result;
}

Params getTestParams() {
  Params result;

  for (const auto &e : fs::directory_iterator("./input")) {
    const auto params = getTestParams(
        e.path().string(), "./output/" + e.path().filename().string());
    result.insert(std::end(result), std::begin(params), std::end(params));
  }

  std::sort(std::begin(result), std::end(result),
            [](const auto &a, const auto &b) { return a.input < b.input; });

  return result;
}

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
  const auto param = GetParam();
  std::cout << param.input << " to " << param.output << std::endl;

  if ((param.type == FileType::ZIP) ||
      (param.type == FileType::PORTABLE_DOCUMENT_FORMAT))
    GTEST_SKIP();

  odr::HtmlConfig config;
  config.editable = true;

  const odr::File file{param.input};

  fs::create_directories(fs::path(param.output));
  auto fileMeta = file.fileMeta();

  // encrypted ooxml type cannot be inspected
  if ((file.fileType() != FileType::OFFICE_OPEN_XML_ENCRYPTED))
    EXPECT_EQ(param.type, file.fileType());

  // TODO
  // EXPECT_EQ(param.encrypted, document.encrypted());
  // if (document.encrypted())
  //  EXPECT_TRUE(document.decrypt(param.password));
  EXPECT_EQ(param.type, file.fileType());

  fileMeta = file.fileMeta();

  {
    const std::string metaOutput = param.output + "/meta.json";
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
      const std::string htmlOutput = param.output + "/document.html";
      fs::create_directories(fs::path(htmlOutput).parent_path());
      // TODO
      //document.translate(htmlOutput, config);
      EXPECT_TRUE(fs::is_regular_file(htmlOutput));
      EXPECT_LT(0, fs::file_size(htmlOutput));
    } else if (document.documentType() == DocumentType::PRESENTATION) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            param.output + "/slide" + std::to_string(i) + ".html";
        // TODO
        //document.translate(htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.documentType() == DocumentType::SPREADSHEET) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            param.output + "/sheet" + std::to_string(i) + ".html";
        // TODO
        //document.translate(htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else if (document.documentType() == DocumentType::GRAPHICS) {
      for (std::uint32_t i = 0; i < documentMeta.entryCount; ++i) {
        config.entryOffset = i;
        config.entryCount = 1;
        const std::string htmlOutput =
            param.output + "/page" + std::to_string(i) + ".html";
        // TODO
        //document.translate(htmlOutput, config);
        EXPECT_TRUE(fs::is_regular_file(htmlOutput));
        EXPECT_LT(0, fs::file_size(htmlOutput));
      }
    } else {
      EXPECT_TRUE(false);
    }
  }
}

INSTANTIATE_TEST_CASE_P(all, DataDrivenTest,
                        testing::ValuesIn(getTestParams()));
