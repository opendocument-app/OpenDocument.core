#include <filesystem>
#include <fstream>
#include <sstream>

#include <odr/html.hpp>

#include <odr/exceptions.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

// A linked stylesheet is of no use to a host serving the service over http if
// the service cannot answer for the path the markup names.
TEST(html, linked_resources_are_served) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const std::string cache_path =
      (std::filesystem::current_path() / "cache").string();

  const auto check = [&](const DecodedFile &file, const std::string &view) {
    HtmlConfig config;
    config.embed_shipped_resources = false;

    const HtmlService service = html::translate(file, cache_path, config);

    std::ostringstream out;
    const HtmlResources resources = service.list_views().at(0).write_html(out);
    ASSERT_EQ(service.list_views().at(0).path(), view);

    std::size_t linked = 0;
    for (const auto &[resource, location] : resources) {
      if (!resource.is_shipped()) {
        continue;
      }
      ASSERT_TRUE(location.has_value()) << resource.name();
      ++linked;

      EXPECT_TRUE(service.exists(*location)) << *location;
      EXPECT_EQ(service.mimetype(*location), resource.mime_type());

      std::ostringstream served;
      service.write(*location, served);
      EXPECT_FALSE(served.str().empty()) << *location;
    }
    EXPECT_GT(linked, 0);
  };

  check(DecodedFile(TestData::test_file_path("odr-public/odt/about.odt"),
                    FileType::zip, logger),
        "files.html");
  check(DecodedFile(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"),
                    logger),
        "text.html");
}

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
