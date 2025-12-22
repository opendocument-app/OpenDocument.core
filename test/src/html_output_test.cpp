#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/util/odr_meta_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <test_util.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

FileType expected_file_type_pre_decryption(const TestFile &test_file) {
  if (test_file.password.has_value()) {
    if (test_file.type == FileType::office_open_xml_document ||
        test_file.type == FileType::office_open_xml_presentation ||
        test_file.type == FileType::office_open_xml_workbook) {
      return FileType::office_open_xml_encrypted;
    }
  }

  return test_file.type;
}

FileType expected_file_type_post_decryption(const TestFile &test_file) {
  return test_file.type;
}

struct TestParams {
  TestFile test_file;
  std::string path;
  DecoderEngine engine{DecoderEngine::odr};
  std::string test_repo;
  std::string output_path;
  std::string output_path_prefix;
};

using HtmlOutputTests = testing::TestWithParam<TestParams>;

TEST_P(HtmlOutputTests, html_meta) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const TestParams &params = GetParam();
  const TestFile &test_file = params.test_file;
  const DecoderEngine engine = params.engine;
  const std::string &output_path = params.output_path;
  const std::string &output_path_prefix = params.output_path_prefix;

  const FileCategory file_category = file_category_by_file_type(test_file.type);

  ODR_INFO(*logger, "Testing file: " << test_file.short_path << " with engine: "
                                     << odr::decoder_engine_to_string(engine)
                                     << " output to: " << output_path);

  // these files cannot be opened
  if (util::string::ends_with(test_file.short_path, ".sxw") ||
      test_file.type == FileType::legacy_powerpoint_presentation ||
      test_file.type == FileType::legacy_excel_worksheets ||
      test_file.type == FileType::word_perfect ||
      test_file.type == FileType::starview_metafile) {
    GTEST_SKIP();
  }

  // TODO fix pdf implementation
  if (engine == DecoderEngine::odr &&
      test_file.type == FileType::portable_document_format &&
      (test_file.short_path.starts_with("odr-private") ||
       test_file.short_path == "odr-public/pdf/Casio_WVA-M650-7AJF.pdf")) {
    GTEST_SKIP();
  }

  DecodePreference decode_preference;
  decode_preference.as_file_type = test_file.type;
  decode_preference.with_engine = engine;
  DecodedFile file = open(test_file.absolute_path, decode_preference, *logger);

  FileMeta file_meta = file.file_meta();

  EXPECT_EQ(file_meta.type, expected_file_type_pre_decryption(test_file));
  if (file_category == FileCategory::document) {
    EXPECT_TRUE(file_meta.document_meta.has_value());
    EXPECT_EQ(file_meta.document_meta->document_type,
              document_type_by_file_type(
                  expected_file_type_pre_decryption(test_file)));
  }

  fs::create_directories(output_path);

  {
    const std::string meta_output = output_path + "/meta.json";
    const nlohmann::json json = util::meta::meta_to_json(file_meta);
    std::ofstream o(meta_output);
    o << std::setw(4) << json << std::endl;
    EXPECT_TRUE(fs::is_regular_file(meta_output));
    EXPECT_LT(0, fs::file_size(meta_output));
  }

  // encrypted ooxml type cannot be inspected
  if (file.file_type() != FileType::office_open_xml_encrypted) {
    EXPECT_EQ(test_file.type, file.file_type());
  }

  // TODO enable zip, csv, json
  if (test_file.type == FileType::zip ||
      test_file.type == FileType::comma_separated_values ||
      test_file.type == FileType::javascript_object_notation) {
    GTEST_SKIP();
  }

  // TODO wvware decryption
  if (test_file.password.has_value() &&
      test_file.type == FileType::legacy_word_document &&
      engine == DecoderEngine::wvware) {
    GTEST_SKIP();
  }

  EXPECT_EQ(test_file.password.has_value(), file.password_encrypted());

  if (test_file.password.has_value()) {
    file = file.decrypt(test_file.password.value());

    // After decryption, the file meta may change
    file_meta = file.file_meta();

    EXPECT_EQ(file_meta.type, expected_file_type_post_decryption(test_file));
    if (file_category == FileCategory::document) {
      EXPECT_TRUE(file_meta.document_meta.has_value());
      EXPECT_EQ(file_meta.document_meta->document_type,
                document_type_by_file_type(
                    expected_file_type_post_decryption(test_file)));
    }

    {
      const std::string meta_output = output_path + "/meta-decrypted.json";
      const nlohmann::json json = util::meta::meta_to_json(file_meta);
      std::ofstream o(meta_output);
      o << std::setw(4) << json << std::endl;
      EXPECT_TRUE(fs::is_regular_file(meta_output));
      EXPECT_LT(0, fs::file_size(meta_output));
    }
  }

  if (file.is_document_file()) {
    DocumentFile document_file = file.as_document_file();

    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  const std::string resource_path =
      Path(output_path_prefix).parent().join(RelPath("resources")).string();
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
  HtmlService service = html::translate(file, output_path_tmp, config);
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
                                      : "-" + decoder_engine_to_string(engine);
}

std::string test_params_to_name(const TestParams &params) {
  std::string path = params.path + engine_suffix(params.engine);
  util::string::replace_all(path, "/", "_");
  util::string::replace_all(path, "-", "_");
  util::string::replace_all(path, "+", "_");
  util::string::replace_all(path, ".", "_");
  util::string::replace_all(path, " ", "_");
  util::string::replace_all(path, "$", "");
  return path;
}

TestParams create_test_params(const TestFile &test_file,
                              const DecoderEngine engine) {
  const std::string test_file_path = test_file.short_path;

  const std::string test_repo = *RelPath(test_file_path).begin();
  const std::string output_path_prefix = AbsPath::current_working_directory()
                                             .join(RelPath("output"))
                                             .join(RelPath(test_repo))
                                             .join(RelPath("output"))
                                             .string();
  const std::string output_path_suffix = engine_suffix(engine);
  const std::string output_path =
      AbsPath(output_path_prefix)
          .join(RelPath(test_file_path).rebase(RelPath(test_repo)))
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
    std::vector engines = {DecoderEngine::odr};
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
