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
  std::string metaOutput;
  std::string htmlOutput;

  Param(std::string input, const FileType type, const bool encrypted,
        std::string password, std::string metaOutput, std::string htmlOutput)
      : input{std::move(input)}, type{type}, encrypted{encrypted},
        password{std::move(password)}, metaOutput{std::move(metaOutput)},
        htmlOutput{std::move(htmlOutput)} {}
};

using Params = std::vector<Param>;

class DataDrivenTest : public testing::TestWithParam<Param> {};

Param getTestParam(std::string input, std::string metaOutput,
                   std::string htmlOutput) {
  const FileType type =
      FileMeta::typeByExtension(access::Path(input).extension());
  const std::string outputName = fs::path(input).filename().replace_extension();
  std::string password;
  if (const auto left = outputName.find('$'), right = outputName.rfind('$');
      (left != std::string::npos) && (left != right))
    password = outputName.substr(left, right);
  const bool encrypted = !password.empty();
  metaOutput += "/" + outputName + ".json";
  htmlOutput += "/" + outputName + ".html";

  return {std::move(input),
          type,
          encrypted,
          std::move(password),
          std::move(metaOutput),
          std::move(htmlOutput)};
}

Params getTestParams(const std::string &input, std::string metaOutput,
                     std::string htmlOutput) {
  if (fs::is_regular_file(input))
    return {getTestParam(input, std::move(metaOutput), std::move(htmlOutput))};
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
      const std::string outputName =
          fs::path(path).filename().replace_extension();
      std::string metaOutputTmp =
          metaOutput + "/" +
          access::Path(row["path"].get<>()).parent().string() + "/" +
          outputName + ".json";
      std::string htmlOutputTmp =
          htmlOutput + "/" +
          access::Path(row["path"].get<>()).parent().string() + "/" +
          outputName + ".html";

      if (type == FileType::UNKNOWN)
        continue;
      result.emplace_back(path, type, encrypted, std::move(password),
                          std::move(metaOutputTmp), std::move(htmlOutputTmp));
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

    std::string metaOutputTmp =
        metaOutput + "/" + access::Path(path).rebase(input).parent().string();
    std::string htmlOutputTmp =
        htmlOutput + "/" + access::Path(path).rebase(input).parent().string();
    const auto param =
        getTestParam(path, std::move(metaOutputTmp), std::move(htmlOutputTmp));

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
        e.path().string(), "./output/meta/" + e.path().filename().string(),
        "./output/html/" + e.path().filename().string());
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
  std::cout << param.input << std::endl;

  if ((param.type == FileType::ZIP) ||
      (param.type == FileType::PORTABLE_DOCUMENT_FORMAT))
    GTEST_SKIP();

  odr::HtmlConfig config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  const odr::File file{param.input};

  // encrypted ooxml type cannot be inspected
  if ((file.fileType() != FileType::OFFICE_OPEN_XML_ENCRYPTED))
    EXPECT_EQ(param.type, file.fileType());

  // TODO
  // EXPECT_EQ(param.encrypted, document.encrypted());
  // if (document.encrypted())
  //  EXPECT_TRUE(document.decrypt(param.password));
  EXPECT_EQ(param.type, file.fileType());

  {
    const auto json = metaToJson(file.fileMeta());

    fs::create_directories(fs::path(param.metaOutput).parent_path());
    std::ofstream o(param.metaOutput);
    o << std::setw(4) << json << std::endl;
  }
  EXPECT_TRUE(fs::is_regular_file(param.metaOutput));
  EXPECT_LT(0, fs::file_size(param.metaOutput));

  // TODO
  // if (!document.translatable())
  //  return;

  fs::create_directories(fs::path(param.htmlOutput).parent_path());
  // TODO
  // document.translate(param.htmlOutput, config);
  EXPECT_TRUE(fs::is_regular_file(param.htmlOutput));
  EXPECT_LT(0, fs::file_size(param.htmlOutput));
}

INSTANTIATE_TEST_CASE_P(all, DataDrivenTest,
                        testing::ValuesIn(getTestParams()));
