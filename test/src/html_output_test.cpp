#include <odr/html.hpp>
#include <odr/html_service.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/util/odr_meta_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <test_util.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::common;
using namespace odr::test;
namespace fs = std::filesystem;

struct TestParams {
  TestFile test_file;
  std::string path;
  DecoderEngine engine{DecoderEngine::odr};
  std::string test_repo;
  std::string output_path;
  std::string output_path_prefix;
};

using HtmlOutputTests = ::testing::TestWithParam<TestParams>;

TEST_P(HtmlOutputTests, html_meta) {
  const TestParams &params = GetParam();
  const TestFile &test_file = params.test_file;
  const DecoderEngine engine = params.engine;
  const std::string &test_repo = params.test_repo;
  const std::string &output_path = params.output_path;
  const std::string &output_path_prefix = params.output_path_prefix;

  std::cout << test_file.short_path << " to " << output_path << std::endl;

  // TODO compare guessed file type VS actual file type

  // these files cannot be opened
  if (util::string::ends_with(test_file.short_path, ".sxw") ||
      (test_file.type == FileType::legacy_powerpoint_presentation) ||
      (test_file.type == FileType::legacy_excel_worksheets) ||
      (test_file.type == FileType::word_perfect) ||
      (test_file.type == FileType::starview_metafile)) {
    GTEST_SKIP();
  }

  // TODO fix
  if ((engine == DecoderEngine::odr) &&
      (test_file.type == FileType::portable_document_format) &&
      (test_repo != "odr-public")) {
    GTEST_SKIP();
  }

  DecodePreference decode_preference;
  decode_preference.as_file_type = test_file.type;
  decode_preference.with_engine = engine;
  DecodedFile file = odr::open(test_file.absolute_path, decode_preference);

  FileMeta file_meta = file.file_meta();

  // encrypted ooxml type cannot be inspected
  if ((file.file_type() != FileType::office_open_xml_encrypted)) {
    EXPECT_EQ(test_file.type, file.file_type());
  }

  // TODO enable zip, csv, json
  if ((test_file.type == FileType::zip) ||
      (test_file.type == FileType::comma_separated_values) ||
      (test_file.type == FileType::javascript_object_notation)) {
    GTEST_SKIP();
  }

  // TODO wvware decryption
  if (test_file.password.has_value() &&
      (test_file.type == FileType::legacy_word_document) &&
      (engine == DecoderEngine::wvware)) {
    GTEST_SKIP();
  }

  EXPECT_EQ(test_file.password.has_value(), file.password_encrypted());

  if (test_file.password.has_value()) {
    file = file.decrypt(test_file.password.value());
  }

  if (file.is_document_file()) {
    DocumentFile document_file = file.document_file();

    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  fs::create_directories(output_path);
  file_meta = file.file_meta();

  {
    const std::string meta_output = output_path + "/meta.json";
    const nlohmann::json json =
        odr::internal::util::meta::meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
  }

  const std::string resource_path = common::Path(output_path_prefix)
                                        .parent()
                                        .join(Path("resources"))
                                        .string();
  std::filesystem::copy(TestData::resource_directory(), resource_path,
                        fs::copy_options::recursive |
                            fs::copy_options::overwrite_existing);

  HtmlConfig config(output_path);
  config.embed_images = true;
  config.embed_shipped_resources = false;
  config.resource_path = resource_path;
  config.relative_resource_paths = true;
  config.editable = true;
  config.spreadsheet_limit = TableDimensions(4000, 500);
  config.format_html = true;
  config.html_indent = 2;

  std::string output_path_tmp = output_path + "/tmp";
  std::filesystem::create_directories(output_path_tmp);
  HtmlService service = odr::html::translate(file, output_path_tmp, config);
  Html html = service.bring_offline(output_path);
  std::filesystem::remove_all(output_path_tmp);

  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

namespace {

std::string engine_suffix(const DecoderEngine engine) {
  return engine == DecoderEngine::odr ? ""
                                      : "-" + odr::engine_to_string(engine);
}

std::string test_params_to_name(const TestParams &params) {
  std::string path = params.path + engine_suffix(params.engine);
  internal::util::string::replace_all(path, "/", "_");
  internal::util::string::replace_all(path, "-", "_");
  internal::util::string::replace_all(path, "+", "_");
  internal::util::string::replace_all(path, ".", "_");
  internal::util::string::replace_all(path, " ", "_");
  internal::util::string::replace_all(path, "$", "");
  return path;
}

TestParams create_test_params(const TestFile &test_file,
                              const DecoderEngine engine) {
  const std::string test_file_path = test_file.short_path;

  const std::string test_repo = *common::Path(test_file_path).begin();
  const std::string output_path_prefix = common::Path("output")
                                             .join(Path(test_repo))
                                             .join(Path("output"))
                                             .string();
  const std::string output_path_suffix = engine_suffix(engine);
  const std::string output_path =
      common::Path(output_path_prefix)
          .join(common::Path(test_file_path).rebase(Path(test_repo)))
          .string() +
      output_path_suffix;

  return {
      .test_file = test_file,
      .path = test_file_path,
      .engine = engine,
      .test_repo = test_repo,
      .output_path = output_path,
      .output_path_prefix = output_path_prefix,
  };
}

std::vector<TestParams> list_test_params() {
  std::vector<TestParams> params;
  for (const TestFile &test_file : TestData::test_files()) {
    std::vector<DecoderEngine> engines = {DecoderEngine::odr};
    if (test_file.type == FileType::portable_document_format) {
      engines.push_back(DecoderEngine::poppler);
    }
    if (test_file.type == FileType::legacy_word_document) {
      engines.clear();
      engines.push_back(DecoderEngine::wvware);
    }

    for (const DecoderEngine engine : engines) {
      params.push_back(create_test_params(test_file, engine));
    }
  }
  return params;
}

} // namespace

INSTANTIATE_TEST_SUITE_P(all_test_files, HtmlOutputTests,
                         testing::ValuesIn(list_test_params()),
                         [](const ::testing::TestParamInfo<TestParams> &info) {
                           return test_params_to_name(info.param);
                         });
