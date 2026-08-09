#include <filesystem>
#include <fstream>

#include <odr/html.hpp>

#include <odr/exceptions.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

// The one archive the reference-output suite renders has no directory in it.
TEST(html, archive_listing) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DecodedFile file(TestData::test_file_path("odr-public/odt/about.odt"),
                         FileType::zip, logger);
  ASSERT_TRUE(file.is_archive_file());

  const std::string output_path =
      (std::filesystem::current_path() / "archive").string();
  const HtmlConfig config(output_path);
  const Html html =
      html::translate(file, config, logger).bring_offline(output_path);

  ASSERT_EQ(html.pages().size(), 1);
  std::ifstream in(html.pages().front().path);
  const std::string page((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

  EXPECT_NE(page.find(R"(<table class="odr-files">)"), std::string::npos);
  // no header row: the listing carries no words to translate
  EXPECT_EQ(page.find("<thead>"), std::string::npos);
  EXPECT_NE(page.find(R"(<td class="odr-files-name">/mimetype</td>)"),
            std::string::npos);
  // a directory says so by its trailing separator and carries no size
  EXPECT_NE(page.find(R"(<tr class="odr-files-directory">)"),
            std::string::npos);
  EXPECT_NE(
      page.find(R"(<td class="odr-files-name">/Configurations2/menubar/</td>)"),
      std::string::npos);
}

TEST(html, views) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ods/Senza nome 1.ods"), logger);

  const Document document = document_file.document();

  const std::string cache_path =
      (std::filesystem::current_path() / "cache").string();
  const HtmlConfig config;
  const HtmlService service = html::translate(document, cache_path, config);

  const HtmlViews &views = service.list_views();

  EXPECT_EQ(views.size(), 3);
  EXPECT_EQ(views.at(0).name(), "document");
  EXPECT_EQ(views.at(1).name(), "Foglio1");
  EXPECT_EQ(views.at(2).name(), "Foglio2");
}
