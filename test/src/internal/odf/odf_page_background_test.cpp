#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

constexpr const char *namespaces =
    R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
    R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
    R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
    R"(xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" )"
    R"(xmlns:presentation="urn:oasis:names:tc:opendocument:xmlns:presentation:1.0" )"
    R"(xmlns:xlink="http://www.w3.org/1999/xlink")";

/// A 1x1 red png.
constexpr std::string_view png =
    "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00"
    "\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90\x77\x53\xde\x00\x00\x00"
    "\x0c\x49\x44\x41\x54\x78\x9c\x63\xf8\xcf\xc0\x00\x00\x03\x01\x01\x00\xc9"
    "\xfe\x92\xef\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82"sv;

/// A presentation whose master page `M` is filled by @p master_fill and whose
/// slides are filled by @p slide_fills, each a `style:drawing-page-properties`
/// attribute list. `Pictures/red.png` is the fill image `Red`, and `G` a
/// gradient.
std::string write_odp(const std::string &name, const std::string &master_fill,
                      const std::vector<std::string> &slide_fills) {
  std::string slide_styles;
  std::string slides;
  for (std::size_t i = 0; i < slide_fills.size(); ++i) {
    const std::string style = "dp" + std::to_string(i);
    slide_styles += R"(<style:style style:name=")" + style +
                    R"(" style:family="drawing-page">)"
                    R"(<style:drawing-page-properties )" +
                    slide_fills[i] + "/></style:style>";
    slides += R"(<draw:page draw:style-name=")" + style +
              R"(" draw:master-page-name="M"/>)";
  }
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )"s +
      namespaces + "><office:automatic-styles>" + slide_styles +
      "</office:automatic-styles><office:body><office:presentation>" + slides +
      "</office:presentation></office:body></office:document-content>";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )"s +
      namespaces +
      R"(><office:styles>)"
      R"(<draw:gradient draw:name="G" draw:style="linear" )"
      R"(draw:start-color="#000000" draw:end-color="#0000ff"/>)"
      R"(<draw:fill-image draw:name="Red" xlink:href="Pictures/red.png"/>)"
      R"(</office:styles><office:automatic-styles>)"
      R"(<style:page-layout style:name="PM"><style:page-layout-properties )"
      R"(fo:page-width="28cm" fo:page-height="21cm"/>)"
      R"(</style:page-layout>)"
      R"(<style:style style:name="Mdp" style:family="drawing-page">)"
      R"(<style:drawing-page-properties )" +
      master_fill +
      R"(/></style:style></office:automatic-styles><office:master-styles>)"
      R"(<style:master-page style:name="M" style:page-layout-name="PM" )"
      R"(draw:style-name="Mdp"/></office:master-styles>)"
      R"(</office:document-styles>)";

  const std::string path =
      (std::filesystem::temp_directory_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.presentation"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles));
  zip.insert_file(std::end(zip), RelPath("Pictures/red.png"),
                  std::make_shared<MemoryFile>(std::string(png)));
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

std::vector<GraphicStyle> backgrounds_of(const std::string &path) {
  const odr::Document document = odr::open(path).as_document_file().document();
  std::vector<GraphicStyle> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_slide().background());
  }
  return result;
}

/// The html of the first slide of the file at @p path.
std::string render(const std::string &path) {
  std::ostringstream html;
  html::translate(odr::open(path), HtmlConfig())
      .list_views()
      .at(1)
      .write_html(html);
  return html.str();
}

} // namespace

TEST(OdfPageBackground, a_slide_takes_its_own_bitmap) {
  const std::string path =
      write_odp("odf_page_background_bitmap.odp", R"(draw:fill="none")",
                {R"(draw:fill="bitmap" draw:fill-image-name="Red" )"
                 R"(style:repeat="stretch")"});

  const GraphicStyle background = backgrounds_of(path).at(0);

  ASSERT_TRUE(background.fill_bitmap.has_value());
  EXPECT_EQ(background.fill_bitmap->repeat, BitmapRepeat::stretch);
  EXPECT_EQ(background.fill_bitmap->file.size(), png.size());

  const std::string html = render(path);
  EXPECT_NE(html.find("background-image:url('data:image/png;base64,"),
            std::string::npos);
  EXPECT_NE(html.find("background-size:100% 100%"), std::string::npos);
  // the dark sheet keeps a picture
  EXPECT_EQ(html.find("odr-fill"), std::string::npos);
}

// As LibreOffice renders them: a fill color alone is solid, a fill image name
// alone and an explicit none show the master's.
TEST(OdfPageBackground, a_slide_without_a_fill_shows_its_master_page_fill) {
  const std::string path =
      write_odp("odf_page_background_master.odp",
                R"(draw:fill="gradient" draw:fill-gradient-name="G")",
                {R"(draw:fill="none")", R"(draw:fill-image-name="Red")", "",
                 R"(draw:fill-color="#ff0000")"});

  const std::vector<GraphicStyle> backgrounds = backgrounds_of(path);

  EXPECT_TRUE(backgrounds.at(0).fill_gradient.has_value());
  EXPECT_TRUE(backgrounds.at(1).fill_gradient.has_value());
  EXPECT_TRUE(backgrounds.at(2).fill_gradient.has_value());
  EXPECT_FALSE(backgrounds.at(3).fill_gradient.has_value());
  ASSERT_TRUE(backgrounds.at(3).fill_color.has_value());
  EXPECT_EQ(backgrounds.at(3).fill_color->rgb(), 0xff0000u);
  EXPECT_EQ(backgrounds.at(3).fill_color->alpha, 255);

  const std::string html = render(path);
  EXPECT_NE(html.find("background-image:linear-gradient("), std::string::npos);
  // the dark sheet clears it
  EXPECT_NE(html.find(R"(class="odr-page-outer odr-fill)"), std::string::npos);
}

TEST(OdfPageBackground, background_visible_false_hides_the_master_page_fill) {
  const std::string path =
      write_odp("odf_page_background_hidden.odp",
                R"(draw:fill="gradient" draw:fill-gradient-name="G")",
                {R"(presentation:background-visible="false")"});

  const GraphicStyle background = backgrounds_of(path).at(0);

  EXPECT_FALSE(background.fill_gradient.has_value());
  EXPECT_FALSE(background.fill_color.has_value());
}
