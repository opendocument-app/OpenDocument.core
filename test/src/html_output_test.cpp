#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/util/odr_meta_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <test_info.hpp>
#include <test_util.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;
namespace fs = std::filesystem;

namespace {

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

/// A deviation from the config the reference output is rendered with, applied
/// last. A file that carries one is rendered twice: the default output keeps
/// its path, the variant's goes to a sibling directory suffixed with the
/// variant's name.
struct ConfigVariant {
  std::string name;
  std::function<void(HtmlConfig &)> apply{[](HtmlConfig &) {}};
};

struct TestParams {
  TestFile test_file;
  std::string path;
  ConfigVariant variant;
  std::string test_repo;
  std::string output_path;
  std::string output_path_prefix;
};

} // namespace

using HtmlOutputTests = testing::TestWithParam<TestParams>;

TEST_P(HtmlOutputTests, html_meta) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const TestParams &params = GetParam();
  const TestFile &test_file = params.test_file;
  const std::string &output_path = params.output_path;
  const std::string &output_path_prefix = params.output_path_prefix;

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

  // TODO enable json
  if (test_file.type == FileType::javascript_object_notation) {
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
  // Linked once per test repository, so a stylesheet change moves those files
  // rather than every document.
  config.embed_shipped_resources = false;
  config.resource_path =
      Path(output_path_prefix).parent().join(RelPath("resources")).string();
  config.relative_resource_paths = true;
  config.editable = true;
  // Keep a text document's page margins instead of reflowing it to the
  // viewport. The Android viewer turns this on by default, so leaving it off
  // here left the reference output showing a rendering path most readers never
  // see — including the page box and the backdrop it brings with it.
  config.text_document_margin = true;
  config.spreadsheet_limit = TableDimensions(4000, 500);
  config.page_range_end = 100;
  config.format_html = true;
  config.html_indent = 1;
  config.html_indent_string = "\t";
  params.variant.apply(config);

  const std::string output_path_tmp = output_path + "/tmp";
  fs::create_directories(output_path_tmp);
  HtmlService service = html::translate(file, output_path_tmp, config);
  Html html = service.bring_offline(output_path);
  fs::remove_all(output_path_tmp);

  for (const HtmlPage &html_page : html.pages()) {
    EXPECT_TRUE(fs::is_regular_file(html_page.path));
    EXPECT_LT(0, fs::file_size(html_page.path));
  }
}

namespace {

/// The default config carries no suffix, so the reference output rendered with
/// it keeps its path; a variant is disambiguated with `-{name}`.
std::string variant_suffix(const ConfigVariant &variant) {
  return variant.name.empty() ? "" : "-" + variant.name;
}

std::string test_params_to_name(const TestParams &params) {
  std::string path = params.path + variant_suffix(params.variant);
  util::string::replace_all(path, "/", "_");
  util::string::replace_all(path, "-", "_");
  util::string::replace_all(path, "+", "_");
  util::string::replace_all(path, ".", "_");
  util::string::replace_all(path, " ", "_");
  util::string::replace_all(path, "$", "");
  return path;
}

TestParams create_test_params(const TestFile &test_file,
                              const ConfigVariant &variant) {
  const std::string test_file_path = test_file.short_path;

  const std::string test_repo = *RelPath(test_file_path).begin();
  const std::string output_path_prefix = AbsPath::current_working_directory()
                                             .join(RelPath("output"))
                                             .join(RelPath(test_repo))
                                             .join(RelPath("output"))
                                             .string();
  const std::string output_path =
      AbsPath(output_path_prefix)
          .join(RelPath(test_file_path).rebase(RelPath(test_repo)))
          .string() +
      variant_suffix(variant);

  return {
      .test_file = test_file,
      .path = test_file_path,
      .variant = variant,
      .test_repo = test_repo,
      .output_path = output_path,
      .output_path_prefix = output_path_prefix,
  };
}

/// The extra configs to render in, each paired with the file to render. A
/// variant is meant to cover a rendering path the default config never takes,
/// so it is pinned to as few files as cover that path — for a color scheme
/// that is one file per *view*, each of which carries its own dark stylesheet,
/// rather than one per format.
std::vector<std::pair<std::string, ConfigVariant>> list_variant_cases() {
  const ConfigVariant single{"single", [](HtmlConfig &config) {
                               config.pdf_text_mode = PdfTextMode::single_layer;
                             }};
  const ConfigVariant dark{"dark", [](HtmlConfig &config) {
                             config.color_scheme = HtmlColorScheme::dark;
                           }};
  const ConfigVariant system{"system", [](HtmlConfig &config) {
                               config.color_scheme = HtmlColorScheme::system;
                             }};
  const ConfigVariant reflow{"reflow", [](HtmlConfig &config) {
                               config.text_document_margin = false;
                             }};
  const ConfigVariant read_only{
      "read-only", [](HtmlConfig &config) { config.editable = false; }};

  return {
      // The text mode only affects the pdf view, and only the odr engine
      // renders one.
      {"odr-private/pdf/978-3-030-65771-0.pdf", single},

      // One file per view honoring a color scheme. The pdf view honors none.
      {"odr-public/odt/style-various-1.odt", dark},
      {"odr-public/odp/style-various-1.odp", dark},
      {"odr-public/ods/style-border-1.ods", dark},
      {"odr-public/txt/lorem ipsum.txt", dark},
      {"odr-public/zip/small.zip", dark},
      {"odr-private/otf/OpenSans-Regular.otf", dark},
      // The image view paints the ground the picture sits on and nothing else,
      // so the file that shows it is one with transparency.
      {"odr-public/png/tango-example-icons.png", dark},
      // `system` writes what `dark` writes, wrapped in a media query; one file
      // pins that wrapper.
      {"odr-public/txt/lorem ipsum.txt", system},

      // A text document reflowed to the viewport rather than kept in its page
      // box — the default the library ships, and the one the reference output
      // stopped showing when the margins were turned on here.
      {"odr-public/odt/about.odt", reflow},
      {"odr-public/docx/physics.docx", reflow},

      // The output a reader gets rather than an editor.
      {"odr-public/odt/style-various-1.odt", read_only},
  };
}

std::vector<TestParams> list_test_params() {
  const std::vector<std::pair<std::string, ConfigVariant>> variant_cases =
      list_variant_cases();

  std::vector<TestParams> params;
  for (const TestFile &test_file : TestData::test_files()) {
    params.push_back(create_test_params(test_file, {}));

    for (const auto &[path, variant] : variant_cases) {
      if (path == test_file.short_path) {
        params.push_back(create_test_params(test_file, variant));
      }
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
