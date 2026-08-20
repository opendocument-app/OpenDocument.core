#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <odr/html.hpp>

#include <odr/exceptions.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

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

std::string render(const std::string &path, const HtmlConfig &config) {
  const DecodedFile file(TestData::test_file_path(path), Logger::null());

  std::ostringstream out;
  html::translate(file, config).list_views().at(0).write_html(out);
  return out.str();
}

std::string render_odt(const HtmlConfig &config) {
  return render("odr-public/odt/about.odt", config);
}

} // namespace

// Reflowed to the viewport there is no page box to inset the text.
TEST(html, flowing_text_is_inset_from_the_screen_edge) {
  HtmlConfig config;
  config.text_document_margin = false;

  const std::string page = render_odt(config);

  EXPECT_NE(page.find(R"(<div class="odr-text-flow">)"), std::string::npos);
  EXPECT_NE(page.find(".odr-text-flow{padding:"), std::string::npos);
}

// For `system` it is the element carrying the dark style that is gated, not a
// rule inside it.
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
          R"html(<link rel="stylesheet" href="document-dark.css" media="(prefers-color-scheme: dark)"/>)html"),
      std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(resources, [](const auto &entry) {
    return entry.second.has_value() && *entry.second == "document-dark.css";
  }));
  EXPECT_TRUE(service.exists("document-dark.css"));
}

// Every view but the pdf one turns over.
TEST(html, color_scheme_reaches_every_view) {
  HtmlConfig config;
  config.color_scheme = HtmlColorScheme::dark;

  // a text file, and the gutter it numbers its lines in
  const std::string text = render("odr-public/txt/lorem ipsum.txt", config);
  EXPECT_NE(text.find("--odr-text-gutter:#161b22"), std::string::npos);

  // a source view
  const DecodedFile xml_file(File::from_memory("<a><b>c</b></a>"),
                             FileType::xml);
  std::ostringstream xml;
  html::translate(xml_file, config).list_views().at(0).write_html(xml);
  EXPECT_NE(xml.str().find("--odr-xml-name:#7ee787"), std::string::npos);

  // a file listing: the archive view of a zip
  const DecodedFile archive(
      TestData::test_file_path("odr-public/odt/about.odt"), FileType::zip,
      Logger::null());
  std::ostringstream listing;
  html::translate(archive, config).list_views().at(0).write_html(listing);
  EXPECT_NE(listing.str().find("--odr-files-link:#6cb6ff"), std::string::npos);

  // the search mark turns its text over wherever a view paints one
  EXPECT_NE(text.find("mark{color:#0d1117!important}"), std::string::npos);
  EXPECT_NE(listing.str().find("mark{color:#0d1117!important}"),
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

// #708: a viewport meta tag is honoured for the top-level document only, so an
// embedder rendering into a frame got no fit at all. #706: and whatever fits
// has to hold the reader's place when the viewport changes under it.
TEST(html, paged_output_fits_the_viewport) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DecodedFile file(
      TestData::test_file_path("odr-public/odp/style-various-1.odp"), logger);

  const auto render = [&](const HtmlConfig &config) {
    const std::string cache =
        (std::filesystem::current_path() / "fit").string();
    std::ostringstream out;
    html::translate(file, cache, config).list_views().at(0).write_html(out);
    return std::move(out).str();
  };

  {
    // Nothing said how wide the output will be shown, so it measures itself.
    const std::string html = render(HtmlConfig());
    EXPECT_NE(html.find("body.style.zoom"), std::string::npos);
    EXPECT_EQ(html.find("body{zoom:"), std::string::npos);
  }

  {
    // A slide is 28cm wide here, well over the 400 css pixels configured.
    HtmlConfig config;
    config.viewport_width = 400;
    const std::string html = render(config);
    EXPECT_NE(html.find("body{zoom:0."), std::string::npos);
    // no script: the factor is in the css, which is the point of configuring it
    EXPECT_EQ(html.find("body.style.zoom"), std::string::npos);
  }

  {
    // Told not to fit, neither half applies.
    HtmlConfig config;
    config.viewport_mode = HtmlViewportMode::actual_size;
    config.viewport_width = 400;
    const std::string html = render(config);
    EXPECT_EQ(html.find("body{zoom:"), std::string::npos);
    EXPECT_EQ(html.find("body.style.zoom"), std::string::npos);
  }
}

// A page turned by `/Rotate` is as wide as the reader sees it, so one document
// can hold pages of different widths — which is how mixed widths turn up in the
// wild. Each view is then fitted to the page it renders, not to the widest.
TEST(html, each_view_fits_the_page_it_renders) {
  test::pdf::PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>")
      // 612pt wide as it stands
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      // 792pt tall on paper, but a quarter turn makes it 1224pt wide on screen
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 792 1224] "
              "/Rotate 90 >>");

  const std::string path =
      (std::filesystem::current_path() / "mixed_rotation.pdf").string();
  {
    std::ofstream out(path, std::ios::binary);
    out << builder.trailer("/Root 1 0 R").build_classic();
  }

  HtmlConfig config;
  config.viewport_width = 400;

  const DecodedFile file{path};
  const HtmlService service = html::translate(
      file, (std::filesystem::current_path() / "rotate").string(), config);

  const auto factor_of = [&](const std::size_t view) {
    std::ostringstream out;
    service.list_views().at(view).write_html(out);
    const std::string html = std::move(out).str();
    const std::size_t at = html.find("body{zoom:");
    EXPECT_NE(at, std::string::npos);
    return std::stod(html.substr(at + 10));
  };

  const double document_view = factor_of(0);
  const double narrow_page = factor_of(1);
  const double wide_page = factor_of(2);

  // the narrow page is scaled down less than the wide one
  EXPECT_GT(narrow_page, wide_page);
  // and the view holding both is fitted to the wide one
  EXPECT_DOUBLE_EQ(document_view, wide_page);
  // 400 / (612pt + 32px) and 400 / (1224pt + 32px), in css pixels
  EXPECT_NEAR(narrow_page, 400.0 / (612 * 96.0 / 72 + 32), 1e-6);
  EXPECT_NEAR(wide_page, 400.0 / (1224 * 96.0 / 72 + 32), 1e-6);
}
