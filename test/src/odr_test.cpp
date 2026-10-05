#include <odr/document.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>

#include <test_util.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

namespace {

/// Every `FileType`, derived from the enum itself rather than from the table —
/// otherwise a type missing from the table would be missing from the test too.
std::vector<FileType> every_file_type() {
  std::vector<FileType> result;
  for (auto i = static_cast<std::size_t>(FileType::unknown);
       i <= static_cast<std::size_t>(FileType::hypertext_markup_language);
       ++i) {
    result.push_back(static_cast<FileType>(i));
  }
  return result;
}

} // namespace

/// `main` carries no version — only a release build stamps one (see AGENTS.md).
TEST(odr, version) { EXPECT_TRUE(odr::version().empty()); }

TEST(odr, commit) { EXPECT_FALSE(odr::commit_hash().empty()); }

TEST(odr, types_odt) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path = TestData::test_file_path("odr-public/odt/about.odt");
  const auto types = list_file_types(path, logger);
  ASSERT_EQ(types.size(), 2);
  EXPECT_EQ(types[0], FileType::zip);
  EXPECT_EQ(types[1], FileType::opendocument_text);

  const auto mime = mimetype(path, logger);
  EXPECT_EQ(mime, "application/vnd.oasis.opendocument.text");
}

TEST(odr, types_wpd) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path =
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  const auto types = list_file_types(path, logger);
  ASSERT_EQ(types.size(), 1);
  EXPECT_EQ(types[0], FileType::word_perfect);

  // wpd has a MIME type in the table now, so both paths agree
  EXPECT_EQ(mimetype(path, logger), "application/vnd.wordperfect");
}

/// Markdown has no signature, so only the name can offer it.
TEST(odr, types_md) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path = TestData::test_file_path("odr-public/md/feature-matrix.md");
  const auto types = list_file_types(path, logger);
  ASSERT_FALSE(types.empty());
  EXPECT_EQ(types.front(), FileType::text_file);
  EXPECT_EQ(types.back(), FileType::markdown);

  // the name only adds a candidate; opening by path takes it
  EXPECT_EQ(open(path, {}, logger).file_type(), FileType::markdown);
  EXPECT_EQ(mimetype(path, logger), "text/markdown");
}

/// A name claims nothing on its own — the bytes still decide.
TEST(odr, a_misnamed_file_is_what_its_bytes_are) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path = TestData::test_file_path("odr-public/odt/about.odt");
  std::ostringstream bytes;
  File::from_disk(path).pipe(bytes);
  for (const std::string name : {"wrong.md", "wrong.pdf"}) {
    EXPECT_EQ(
        open(File::from_memory(bytes.str(), name), {}, logger).file_type(),
        FileType::opendocument_text);
  }

  // Markdown needs a name hint.
  const DecodedFile from_memory =
      open(File::from_memory("# heading\n"), {}, logger);
  EXPECT_EQ(from_memory.file_type(), FileType::text_file);
}

/// A caller who holds bytes rather than a path can still say what they were
/// called, and that name offers the same candidate a path would have.
TEST(odr, a_named_file_in_memory_is_offered_its_type) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const File file = File::from_memory("# heading\n", "notes.md");

  const auto types = list_file_types(file, logger);
  ASSERT_FALSE(types.empty());
  EXPECT_EQ(types.front(), FileType::text_file);
  EXPECT_EQ(types.back(), FileType::markdown);

  EXPECT_EQ(open(file, {}, logger).file_type(), FileType::markdown);
  EXPECT_EQ(mimetype(file, logger), "text/markdown");
}

TEST(FileTypeTable, covers_every_file_type_exactly_once) {
  const std::vector<FileType> expected = every_file_type();
  const std::span<const FileType> actual = all_file_types();

  EXPECT_TRUE(std::ranges::equal(actual, expected));

  for (const FileType type : expected) {
    EXPECT_NE(file_type_to_string(type), "unnamed")
        << "file type " << static_cast<int>(type) << " has no table row";
  }
}

TEST(FileTypeTable, aliases_are_unique_across_file_types) {
  std::set<std::string_view> extensions;
  std::set<std::string_view> mimetypes;

  for (const FileType type : every_file_type()) {
    for (const std::string_view extension :
         file_extensions_by_file_type(type)) {
      EXPECT_TRUE(extensions.insert(extension).second)
          << "extension " << extension << " is claimed by two file types";
    }
    for (const std::string_view mimetype : mimetypes_by_file_type(type)) {
      EXPECT_TRUE(mimetypes.insert(mimetype).second)
          << "mimetype " << mimetype << " is claimed by two file types";
    }
  }
}

TEST(FileTypeTable, every_alias_maps_back_to_its_file_type) {
  for (const FileType type : every_file_type()) {
    for (const std::string_view extension :
         file_extensions_by_file_type(type)) {
      EXPECT_EQ(file_type_by_file_extension(std::string(extension)), type)
          << "extension " << extension;
    }
    for (const std::string_view mimetype : mimetypes_by_file_type(type)) {
      EXPECT_EQ(file_type_by_mimetype(mimetype), type)
          << "mimetype " << mimetype;
    }
  }
}

TEST(FileTypeTable, canonical_alias_is_the_first_one) {
  for (const FileType type : every_file_type()) {
    const auto extensions = file_extensions_by_file_type(type);
    if (extensions.empty()) {
      EXPECT_THROW(std::ignore = file_extension_by_file_type(type),
                   UnsupportedFileType);
    } else {
      EXPECT_EQ(file_extension_by_file_type(type), extensions.front());
    }

    const auto mimetypes = mimetypes_by_file_type(type);
    if (mimetypes.empty()) {
      EXPECT_THROW(std::ignore = mimetype_by_file_type(type),
                   UnsupportedFileType);
    } else {
      EXPECT_EQ(mimetype_by_file_type(type), mimetypes.front());
    }
  }
}

/// Without the row an `.html` decodes as xml and shows its own source.
TEST(FileTypeTable, html_is_named_but_not_decoded) {
  const FileType html = FileType::hypertext_markup_language;

  EXPECT_EQ(file_type_by_file_extension("html"), html);
  EXPECT_EQ(file_type_by_file_extension("htm"), html);
  EXPECT_EQ(file_type_by_file_extension("xhtml"), html);
  EXPECT_EQ(file_type_by_mimetype("text/html"), html);
  EXPECT_EQ(mimetype_by_file_type(html), "text/html");
  EXPECT_EQ(file_category_by_file_type(html), FileCategory::text);

  const FileTypeCapabilities capabilities = capabilities_by_file_type(html);
  EXPECT_FALSE(capabilities.detect_by_content);
  EXPECT_FALSE(capabilities.open);
  EXPECT_FALSE(capabilities.translate_html);

  const std::string page = "<!DOCTYPE html><html><body><p>hi</p></body></html>";
  // named but undecodable, and the refusal says which of the two it is
  EXPECT_THROW(std::ignore = open(File::from_memory(page),
                                  DecodeOptions::as(html), Logger::null()),
               UnsupportedFileType);
}

/// `FileType::unknown` is the only type we refuse to name a MIME type for.
TEST(FileTypeTable, only_unknown_has_no_mimetype) {
  for (const FileType type : every_file_type()) {
    if (type == FileType::unknown) {
      EXPECT_THROW(std::ignore = mimetype_by_file_type(type),
                   UnsupportedFileType);
    } else {
      EXPECT_FALSE(mimetype_by_file_type(type).empty())
          << file_type_to_string(type);
    }
  }
}

TEST(FileTypeTable, capabilities_build_on_each_other) {
  for (const FileType type : every_file_type()) {
    const FileTypeCapabilities capabilities = capabilities_by_file_type(type);

    if (!capabilities.open) {
      EXPECT_FALSE(capabilities.translate_html) << file_type_to_string(type);
      EXPECT_FALSE(capabilities.edit) << file_type_to_string(type);
      EXPECT_FALSE(capabilities.save) << file_type_to_string(type);
      EXPECT_FALSE(capabilities.annotate) << file_type_to_string(type);
    }
    if (!capabilities.save) {
      EXPECT_FALSE(capabilities.encrypt) << file_type_to_string(type);
    }
    if (capabilities.edit) {
      EXPECT_TRUE(capabilities.save) << file_type_to_string(type);
    }
    if (capabilities.create) {
      EXPECT_TRUE(capabilities.edit) << file_type_to_string(type);
    }
    if (!capabilities.translate_html) {
      EXPECT_FALSE(capabilities.color_scheme) << file_type_to_string(type);
    }
  }
}

TEST(FileTypeCapabilities, color_scheme_matches_the_html) {
  std::size_t candidates = 0;
  for (const FileType type : every_file_type()) {
    const FileTypeCapabilities declared = capabilities_by_file_type(type);
    if (!declared.translate_html) {
      continue;
    }
    const std::size_t before = candidates;
    bool checked = false;
    std::string error;
    for (const TestFile &test_file : TestData::test_files(type)) {
      if (test_file.password.has_value()) {
        continue;
      }
      ++candidates;
      try {
        const DecodedFile file =
            open(test_file.absolute_path, DecodeOptions::as(type));
        const auto render = [&](const HtmlColorScheme scheme) {
          HtmlConfig config;
          config.color_scheme = scheme;
          config.page_range_end = 1;
          std::ostringstream out;
          html::translate(file, config).list_views().at(0).write_html(out);
          return out.str();
        };
        EXPECT_EQ(render(HtmlColorScheme::light) !=
                      render(HtmlColorScheme::dark),
                  declared.color_scheme)
            << test_file.short_path;
        checked = true;
        break;
      } catch (const std::exception &failure) {
        error = test_file.short_path + ": " + failure.what();
      }
    }
    if (candidates != before) {
      EXPECT_TRUE(checked) << file_type_to_string(type) << ": " << error;
    }
  }
  if (candidates == 0) {
    GTEST_SKIP() << "No unencrypted corpus files available for rendering";
  }
}

/// Engine capabilities must stay within the format's declarations.
TEST(FileTypeCapabilities, declaration_matches_the_engines) {
  constexpr std::size_t files_per_file_type = 2;
  std::size_t candidates = 0;
  for (const FileType type : every_file_type()) {
    const FileTypeCapabilities declared = capabilities_by_file_type(type);
    const std::size_t before = candidates;
    std::size_t checked = 0;
    std::string error;
    for (const TestFile &test_file : TestData::test_files(type)) {
      if (checked == files_per_file_type) {
        break;
      }
      if ((declared.edit || declared.save) && test_file.password.has_value()) {
        continue;
      }
      ++candidates;
      if (!declared.open) {
        EXPECT_ANY_THROW(std::ignore = open(test_file.absolute_path,
                                            DecodeOptions::as(type)))
            << test_file.short_path;
        ++checked;
        continue;
      }

      std::optional<DecodedFile> file;
      std::optional<Document> document;
      try {
        file = open(test_file.absolute_path, DecodeOptions::as(type));
        if (file->is_document_file() && !file->password_encrypted()) {
          document = file->as_document_file().document();
        }
      } catch (const std::exception &failure) {
        error = test_file.short_path + ": " + failure.what();
        continue;
      }
      ++checked;
      const std::vector<FileType> detected =
          list_file_types(test_file.absolute_path);
      if (std::ranges::find(detected, type) != detected.end()) {
        EXPECT_TRUE(declared.detect_by_content ||
                    file_type_by_file_extension(
                        Path(test_file.absolute_path).extension()) == type)
            << test_file.short_path;
      }

      const FileTypeCapabilities declared_actual =
          capabilities_by_file_type(file->file_type());
      const FileTypeCapabilities actual = file->capabilities();
      EXPECT_TRUE(declared_actual.open) << test_file.short_path;
      EXPECT_TRUE(!actual.decrypt || declared_actual.decrypt)
          << test_file.short_path;
      EXPECT_TRUE(!actual.translate_html || declared_actual.translate_html)
          << test_file.short_path;
      if (file->is_text_file()) {
        EXPECT_EQ(file->as_text_file().is_savable(), declared_actual.save)
            << test_file.short_path;
      }
      if (document.has_value()) {
        EXPECT_EQ(document->is_editable(), declared_actual.edit)
            << test_file.short_path;
        EXPECT_EQ(document->is_savable(false), declared_actual.save)
            << test_file.short_path;
        EXPECT_EQ(document->is_savable(true), declared_actual.encrypt)
            << test_file.short_path;
      }
    }
    if (candidates != before) {
      EXPECT_GT(checked, 0u) << file_type_to_string(type) << ": " << error;
    }
  }
  if (candidates == 0) {
    GTEST_SKIP() << "No corpus files available for capability checks";
  }
}
