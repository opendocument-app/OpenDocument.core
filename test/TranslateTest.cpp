#include <gtest/gtest.h>
#include <odr/Config.h>
#include <odr/Document.h>
#include <access/Path.h>
#include <filesystem>
#include <csv.hpp>

using namespace odr;
namespace fs = std::filesystem;

namespace {
struct TranslateParam {
  std::string input;
  std::string password;
  std::string htmlOutput;
  std::string metaOutput;

  TranslateParam(std::string input, std::string password, std::string htmlOutput, std::string metaOutput)
      : input{std::move(input)}, password{std::move(password)}, htmlOutput{std::move(htmlOutput)}, metaOutput{std::move(metaOutput)} {}
};

class TranslateTest : public testing::TestWithParam<TranslateParam> {};

std::vector<TranslateParam> getTestParams(const std::string &input, const std::string &output) {
  if (!fs::is_directory(input))
    return {};

  const std::string index = input + "/index.csv";
  if (!fs::is_regular_file(index))
    return {};

  std::vector<TranslateParam> result;

  for (auto &&row: csv::CSVReader(index)) {
    std::string outputName = fs::path(row["path"].get<>()).filename().replace_extension();
    std::string htmlOutput = output + "/html/" + outputName + ".html";
    std::string metaOutput = output + "/meta/" + outputName + ".json";

    result.emplace_back(row["path"].get<>(), row["password"].get<>(), std::move(htmlOutput), std::move(metaOutput));
  }

  return result;
}

std::vector<TranslateParam> getTestParams() {
  std::vector<TranslateParam> result;

  for (const auto &e : fs::directory_iterator("./input")) {
    const auto params = getTestParams(e.path().string(), "./output/" + e.path().filename().string());
    result.insert(std::end(result), std::begin(params), std::end(params));
  }

  return result;
}
} // namespace

TEST_P(TranslateTest, translate) {
  const auto param = GetParam();

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  const odr::Document document{param.input};

  if (document.encrypted()) {
    EXPECT_TRUE(document.decrypt(param.password));
  }

  document.translate(param.htmlOutput, config);
}

INSTANTIATE_TEST_CASE_P(translate, TranslateTest,
                        testing::ValuesIn(getTestParams()));
