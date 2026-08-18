#include <algorithm>
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
  check(
      DecodedFile(TestData::test_file_path("odr-public/pdf/empty.pdf"), logger),
      "document.html");
}

// The one archive the reference-output suite renders has no directory in it.
// An archive may hold a file named like the stylesheet. Forcing the collision
// through the locator saves needing such an archive in the test data.
TEST(html, archive_entry_yields_to_a_shipped_resource) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DecodedFile file(TestData::test_file_path("odr-public/odt/about.odt"),
                         FileType::zip, logger);

  HtmlConfig config((std::filesystem::current_path() / "collision").string());
  config.embed_shipped_resources = false;
  config.resource_locator = [](const HtmlResource &resource,
                               const HtmlConfig &) -> HtmlResourceLocation {
    return resource.is_shipped() ? "content.xml" : resource.path();
  };

  std::ostringstream out;
  const HtmlResources resources =
      html::translate(file, config, logger).list_views().at(0).write_html(out);

  // The locator puts every shipped resource on that one location, so what the
  // count says is that the entry of that name is not among them.
  std::size_t shipped = 0;
  std::size_t claimants = 0;
  for (const auto &[resource, location] : resources) {
    if (resource.is_shipped()) {
      ++shipped;
    }
    if (location.has_value() && *location == "content.xml") {
      ++claimants;
      EXPECT_TRUE(resource.is_shipped());
    }
  }
  EXPECT_GT(shipped, 0);
  EXPECT_EQ(claimants, shipped);
}

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
  // directories are not listed; every entry names its own whole path
  EXPECT_EQ(page.find("Configurations2/menubar/<"), std::string::npos);
  EXPECT_NE(page.find("/Configurations2/accelerator/current.xml"),
            std::string::npos);

  // entries are written beside the listing and linked, rather than base64'd
  // into it, so the path opens one and the glyph saves it
  EXPECT_EQ(page.find("data:"), std::string::npos);
  EXPECT_NE(page.find(R"(<a href="content.xml" title="content.xml">)"),
            std::string::npos);
  EXPECT_NE(
      page.find(
          R"(<a href="content.xml" download="content.xml" title="content.xml">)"),
      std::string::npos);
  EXPECT_TRUE(std::filesystem::is_regular_file(
      std::filesystem::path(output_path) / "content.xml"));
  EXPECT_TRUE(std::filesystem::is_regular_file(
      std::filesystem::path(output_path) / "Pictures" /
      "10000000000001F4000001FF1D2394A8.jpg"));
}

namespace {

std::string render_odt(const HtmlConfig &config) {
  const DecodedFile file(TestData::test_file_path("odr-public/odt/about.odt"),
                         Logger::null());

  std::ostringstream out;
  html::translate(file, config).list_views().at(0).write_html(out);
  return out.str();
}

} // namespace

// Reflowed to the viewport a text document has no page box to inset it, and the
// body carries no margin of its own.
TEST(html, flowing_text_is_inset_from_the_screen_edge) {
  HtmlConfig config;
  config.text_document_margin = false;

  const std::string page = render_odt(config);

  EXPECT_NE(page.find(R"(<div class="odr-text-flow">)"), std::string::npos);
  EXPECT_NE(page.find(".odr-text-flow{padding:"), std::string::npos);
}

// The colors a document sets are inline, out of reach of a media query, so the
// dark scheme is a stylesheet that overrides them — and for `system` it is the
// element carrying that stylesheet which is gated, not a rule inside it.
TEST(html, color_scheme_writes_the_dark_style) {
  HtmlConfig config;

  EXPECT_EQ(config.color_scheme, HtmlColorScheme::light);
  EXPECT_EQ(render_odt(config).find("color-scheme:dark"), std::string::npos);

  config.color_scheme = HtmlColorScheme::dark;
  const std::string dark = render_odt(config);
  EXPECT_NE(dark.find("color-scheme:dark"), std::string::npos);
  EXPECT_EQ(dark.find("prefers-color-scheme"), std::string::npos);

  config.color_scheme = HtmlColorScheme::system;
  const std::string system = render_odt(config);
  EXPECT_NE(
      system.find(R"html(<style media="(prefers-color-scheme: dark)">)html"),
      std::string::npos);
}

// A linked stylesheet keeps its media query, and the service answers for it.
TEST(html, linked_dark_style_is_served) {
  HtmlConfig config;
  config.embed_shipped_resources = false;
  config.color_scheme = HtmlColorScheme::system;

  const DecodedFile file(TestData::test_file_path("odr-public/odt/about.odt"),
                         Logger::null());
  const HtmlService service = html::translate(file, config);

  std::ostringstream out;
  const HtmlResources resources = service.list_views().at(0).write_html(out);

  EXPECT_NE(
      out.str().find(
          R"html(<link rel="stylesheet" href="dark.css" media="(prefers-color-scheme: dark)"/>)html"),
      std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(resources, [](const auto &entry) {
    return entry.second.has_value() && *entry.second == "dark.css";
  }));
  EXPECT_TRUE(service.exists("dark.css"));
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
