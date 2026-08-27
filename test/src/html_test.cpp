#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

#include <odr/html.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>

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

// Same for a document resource, which unlike a shipped one is named by the
// engine: the reference output embeds every image, so nothing else renders the
// linked form (opendocument-app/OpenDocument.droid#551).
TEST(html, linked_images_are_served) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const std::string cache_path =
      (std::filesystem::current_path() / "images").string();

  const auto check = [&](const std::string &path) {
    const DecodedFile file(TestData::test_file_path(path), logger);

    HtmlConfig config;
    config.embed_images = false;

    const HtmlService service = html::translate(file, cache_path, config);

    std::ostringstream out;
    const HtmlResources resources = service.list_views().at(0).write_html(out);

    std::size_t linked = 0;
    for (const auto &[resource, location] : resources) {
      if (resource.type() != HtmlResourceType::image ||
          !resource.is_accessible()) {
        continue;
      }
      ASSERT_TRUE(location.has_value()) << resource.name();
      ++linked;

      // an absolute location would resolve against the server root rather than
      // against the document
      EXPECT_TRUE(Path(*location).relative()) << *location;
      EXPECT_TRUE(service.exists(*location)) << *location;

      std::ostringstream served;
      service.write(*location, served);
      EXPECT_FALSE(served.str().empty()) << *location;
    }
    EXPECT_GT(linked, 0) << path;
  };

  check("odr-public/odt/image-text-wrap.odt");
  check("odr-public/docx/file-sample_100kB.docx");
  check("odr-public/xlsx/sample.xlsx");
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

// Every view that insets its content declares the floor it was given; unset it
// declares nothing, which leaves the shipped stylesheets alone.
TEST(html, min_content_margin_reaches_every_view) {
  HtmlConfig config;

  const auto render_as = [&](const std::string &path, const FileType as) {
    const DecodedFile file(TestData::test_file_path(path), as, Logger::null());
    std::ostringstream out;
    html::translate(file, config).list_views().at(0).write_html(out);
    return std::move(out).str();
  };

  const auto xml = [&] {
    const DecodedFile file(File::from_memory("<a><b>c</b></a>"), FileType::xml);
    std::ostringstream out;
    html::translate(file, config).list_views().at(0).write_html(out);
    return std::move(out).str();
  };

  const auto every_view = [&] {
    return std::array{
        // a document, and the page column it stacks
        render_odt(config),
        render("odr-public/txt/lorem ipsum.txt", config),
        xml(),
        // a file listing: the archive view of a zip
        render_as("odr-public/odt/about.odt", FileType::zip),
        render_as("odr-public/png/tango-example-icons.png",
                  FileType::portable_network_graphics),
        render_as("odr-public/pdf/empty.pdf",
                  FileType::portable_document_format),
        render_as("odr-private/otf/OpenSans-Regular.otf",
                  FileType::opentype_font),
    };
  };

  for (const std::string &page : every_view()) {
    EXPECT_EQ(page.find("--odr-min-margin-left:"), std::string::npos);
  }

  config.min_content_margin.left = Measure("7px");
  for (const std::string &page : every_view()) {
    EXPECT_NE(page.find(":root{--odr-min-margin-left:7px;}"),
              std::string::npos);
  }
}

namespace {

/// The `--odr-fit` @p page states as a factor, or nothing where it names who
/// measures it instead.
std::optional<double> fit_of(const std::string &page) {
  constexpr std::string_view key = "--odr-fit:";
  const std::size_t at = page.find(key);
  if (at == std::string::npos) {
    return {};
  }
  // `auto` and `view` name a measurer rather than stating a factor
  const std::size_t begin = at + key.length();
  if (begin >= page.length() ||
      std::isdigit(static_cast<unsigned char>(page[begin])) == 0) {
    return {};
  }
  return std::stod(page.substr(begin));
}

} // namespace

// The gutter around the page column is part of the width the view is fitted
// to, so raising it fits the pages smaller.
TEST(html, min_content_margin_widens_the_page_column_fit) {
  HtmlConfig config;
  config.text_document_margin = true;
  config.viewport_width = 400;

  const std::optional<double> narrow = fit_of(render_odt(config));
  ASSERT_TRUE(narrow.has_value());

  config.min_content_margin.left = Measure("100px");
  config.min_content_margin.right = Measure("100px");
  const std::optional<double> wide = fit_of(render_odt(config));
  ASSERT_TRUE(wide.has_value());

  EXPECT_LT(*wide, *narrow);

  // A unit the fit cannot convert leaves it where it was; the css still
  // applies the margin.
  config.min_content_margin.left = Measure("100em");
  config.min_content_margin.right = Measure("100em");
  EXPECT_EQ(fit_of(render_odt(config)), narrow);
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

// #708 (a meta tag does not fit a framed document) and #706 (the fit has to
// hold the reading position).
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
    EXPECT_NE(html.find("--odr-fit:auto"), std::string::npos);
    // nothing to apply yet: the view measures the fit and applies it itself
    EXPECT_EQ(html.find("--odr-zoom:0"), std::string::npos);
  }

  {
    // A slide is 28cm wide here, well over the 400 css pixels configured.
    HtmlConfig config;
    config.viewport_width = 400;
    const std::string html = render(config);
    EXPECT_NE(html.find("--odr-fit:0."), std::string::npos);
    EXPECT_NE(html.find("body{zoom:0."), std::string::npos);
    // the factor is in the css, which is the point of configuring the width;
    // the script is written for the zoom api, not to measure anything
    EXPECT_EQ(html.find("--odr-fit:auto"), std::string::npos);
  }

  {
    // Told not to fit, neither half applies.
    HtmlConfig config;
    config.viewport_mode = HtmlViewportMode::actual_size;
    config.viewport_width = 400;
    const std::string html = render(config);
    // no fit stated at all, and so nothing but the print rule to apply
    EXPECT_EQ(html.find("--odr-fit:"), std::string::npos);
    EXPECT_EQ(html.find("body{zoom:0."), std::string::npos);
  }

  {
    // The meta tag pins the scale so the browser does not fit it as well.
    HtmlConfig config;
    config.viewport_mode = HtmlViewportMode::fit_width_by_view;
    config.viewport_width = 400;
    const std::string html = render(config);
    EXPECT_NE(html.find("--odr-fit:view"), std::string::npos);
    EXPECT_NE(html.find("initial-scale=1.0"), std::string::npos);
    // nothing to open at: the view measures and applies it
    EXPECT_EQ(html.find("body{zoom:0."), std::string::npos);
  }

  {
    // A pinned zoom is what the view opens at, fit or no fit.
    HtmlConfig config;
    config.initial_zoom = 2;
    const std::string html = render(config);
    EXPECT_NE(html.find("--odr-zoom:2"), std::string::npos);
    EXPECT_NE(html.find("body{zoom:2}"), std::string::npos);
    // still measured, so `resetZoom()` has a fit to go back to
    EXPECT_NE(html.find("--odr-fit:auto"), std::string::npos);
  }
}

// `/Rotate` is how one document comes to hold pages of differing width.
TEST(html, each_view_fits_the_page_it_renders) {
  test::pdf::PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>")
      // 612pt wide as it stands
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      // a quarter turn makes this 1224pt wide on screen
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

// An image overflowed its frame the same way a page did. Css alone fits it —
// it has no layout width to preserve — and the reader's zoom rides on top.
TEST(html, an_image_fits_the_viewport) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DecodedFile file(
      TestData::test_file_path("odr-public/png/tango-example-icons.png"),
      logger);

  const auto render = [&](const HtmlConfig &config) {
    const std::string cache =
        (std::filesystem::current_path() / "image_fit").string();
    std::ostringstream out;
    html::translate(file, cache, config).list_views().at(0).write_html(out);
    return std::move(out).str();
  };

  EXPECT_NE(render(HtmlConfig()).find("img{max-width:calc(100% *"),
            std::string::npos);

  HtmlConfig actual_size;
  actual_size.viewport_mode = HtmlViewportMode::actual_size;
  EXPECT_EQ(render(actual_size).find("img{max-width:"), std::string::npos);
}

// A page whose `<img>` no browser decodes is blank with nothing to report, so
// the table declares none and `translate` holds to it.
TEST(html, an_image_no_browser_decodes_is_not_translated) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  for (const FileType type :
       {FileType::jpeg_2000, FileType::photoshop_document,
        FileType::windows_metafile, FileType::enhanced_metafile}) {
    const FileTypeCapabilities capabilities = capabilities_by_file_type(type);
    EXPECT_TRUE(capabilities.open) << file_type_to_string(type);
    EXPECT_FALSE(capabilities.translate_html) << file_type_to_string(type);

    // nothing decodes the bytes, so any will do
    const DecodedFile file =
        open(File::from_memory("image bytes"), type, logger);
    ASSERT_TRUE(file.is_image_file()) << file_type_to_string(type);
    EXPECT_THROW(std::ignore = html::translate(file, HtmlConfig()),
                 UnsupportedFileType)
        << file_type_to_string(type);
  }
}
