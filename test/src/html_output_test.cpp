#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/util/odr_meta_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <test_info.hpp>
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

/// PDF document meta is all-or-nothing: a structure we cannot parse leaves
/// `document_type` unset rather than half-filled (see `pdf/AGENTS.md`), so the
/// per-file answer may fall short of what the format declares.
bool document_type_is_comparable(const TestFile &test_file,
                                 const FileMeta &file_meta) {
  return test_file.type != FileType::portable_document_format ||
         file_meta.document_type != DocumentType::unknown;
}

struct TestParams {
  TestFile test_file;
  std::string path;
  PdfTextMode pdf_text_mode{PdfTextMode::dual_layer};
  std::string test_repo;
  std::string output_path;
};

using HtmlOutputTests = testing::TestWithParam<TestParams>;

TEST_P(HtmlOutputTests, html_meta) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const TestParams &params = GetParam();
  const TestFile &test_file = params.test_file;
  const std::string &output_path = params.output_path;

  const FileCategory file_category = file_category_by_file_type(test_file.type);

  ODR_INFO(logger, "Testing file: " << test_file.short_path
                                    << " output to: " << output_path);

  // formats we cannot decode at all (wpd, rtf, md, …) plus the odd file we
  // classify but do not handle
  if (util::string::ends_with(test_file.short_path, ".sxw") ||
      test_file.type == FileType::starview_metafile ||
      !capabilities_by_file_type(test_file.type).open) {
    GTEST_SKIP();
  }

  DecodePreference decode_preference;
  decode_preference.as_file_type = test_file.type;
  DecodedFile file = open(test_file.absolute_path, decode_preference, logger);

  FileMeta file_meta = file.file_meta();

  EXPECT_EQ(file_meta.type, expected_file_type_pre_decryption(test_file));
  if (file_category == FileCategory::document &&
      document_type_is_comparable(test_file, file_meta)) {
    EXPECT_EQ(file_meta.document_type,
              document_type_by_file_type(
                  expected_file_type_pre_decryption(test_file)));
  }

  fs::create_directories(output_path);

  {
    const std::string meta_output = output_path + "/meta.json";
    const nlohmann::json json = util::meta::meta_to_json(file_meta);
    {
      std::ofstream o(meta_output);
      o << std::setw(4) << json << '\n';
    }
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

  // TODO oldms decryption
  if (test_file.password.has_value() &&
      test_file.type == FileType::legacy_word_document) {
    GTEST_SKIP();
  }

  EXPECT_EQ(test_file.password.has_value(), file.password_encrypted());

  if (test_file.password.has_value()) {
    file = file.decrypt(test_file.password.value());

    // After decryption, the file meta may change
    file_meta = file.file_meta();

    EXPECT_EQ(file_meta.type, expected_file_type_post_decryption(test_file));
    if (file_category == FileCategory::document &&
        document_type_is_comparable(test_file, file_meta)) {
      EXPECT_EQ(file_meta.document_type,
                document_type_by_file_type(
                    expected_file_type_post_decryption(test_file)));
    }

    {
      const std::string meta_output = output_path + "/meta-decrypted.json";
      const nlohmann::json json = util::meta::meta_to_json(file_meta);
      {
        std::ofstream o(meta_output);
        o << std::setw(4) << json << '\n';
      }
      EXPECT_TRUE(fs::is_regular_file(meta_output));
      EXPECT_LT(0, fs::file_size(meta_output));
    }
  }

  if (file.is_document_file()) {
    DocumentFile document_file = file.as_document_file();

    EXPECT_EQ(test_file.type, document_file.file_type());
  }

  HtmlConfig config(output_path);
  config.embed_images = true;
  config.editable = true;
  config.spreadsheet_limit = TableDimensions(4000, 500);
  config.page_range_end = 100;
  config.format_html = true;
  config.html_indent = 1;
  config.html_indent_string = "\t";
  config.pdf_text_mode = params.pdf_text_mode;

  const std::string output_path_tmp = output_path + "/tmp";
  fs::create_directories(output_path);
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

/// The default `dual_layer` mode carries no suffix so existing (non-PDF and
/// dual-layer) reference outputs keep their paths; single-layer variants are
/// disambiguated with `-single`.
std::string text_mode_suffix(const PdfTextMode pdf_text_mode) {
  return pdf_text_mode == PdfTextMode::dual_layer ? "" : "-single";
}

std::string test_params_to_name(const TestParams &params) {
  std::string path = params.path + text_mode_suffix(params.pdf_text_mode);
  util::string::replace_all(path, "/", "_");
  util::string::replace_all(path, "-", "_");
  util::string::replace_all(path, "+", "_");
  util::string::replace_all(path, ".", "_");
  util::string::replace_all(path, " ", "_");
  util::string::replace_all(path, "$", "");
  return path;
}

TestParams create_test_params(const TestFile &test_file,
                              const PdfTextMode pdf_text_mode) {
  const std::string test_file_path = test_file.short_path;

  const std::string test_repo = *RelPath(test_file_path).begin();
  const std::string output_path_prefix = AbsPath::current_working_directory()
                                             .join(RelPath("output"))
                                             .join(RelPath(test_repo))
                                             .join(RelPath("output"))
                                             .string();
  const std::string output_path_suffix = text_mode_suffix(pdf_text_mode);
  const std::string output_path =
      AbsPath(output_path_prefix)
          .join(RelPath(test_file_path).rebase(RelPath(test_repo)))
          .string() +
      output_path_suffix;

  return {
      .test_file = test_file,
      .path = test_file_path,
      .pdf_text_mode = pdf_text_mode,
      .test_repo = test_repo,
      .output_path = output_path,
  };
}

std::vector<TestParams> list_test_params() {
  std::vector<TestParams> params;
  for (const TestFile &test_file : TestData::test_files()) {
    params.push_back(create_test_params(test_file, PdfTextMode::dual_layer));

    // PDFs default to `PdfTextMode::dual_layer`. To keep the single-layer path
    // under reference-output coverage too, eject an extra `-single` test case
    // for one representative PDF (odr engine only, since the text mode only
    // affects odr's PDF rendering).
    if (test_file.short_path == "odr-private/pdf/978-3-030-65771-0.pdf") {
      params.push_back(
          create_test_params(test_file, PdfTextMode::single_layer));
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
