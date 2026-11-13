#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A drawing whose page holds @p body, with the graphic style `gr` filled red
/// and outlined yellow.
std::string write_odg(const std::string &name, const std::string &body) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0">)"
      R"(<office:automatic-styles>)"
      R"(<style:style style:name="gr" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid" draw:fill-color="#990000" )"
      R"(draw:stroke="solid" svg:stroke-color="#ffff00"/></style:style>)"
      R"(</office:automatic-styles><office:body><office:drawing><draw:page>)" +
      body +
      R"(</draw:page></office:drawing></office:body></office:document-content>)";

  const std::string path =
      (std::filesystem::temp_directory_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.graphics"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

/// A custom shape holding @p text, with the enhanced geometry attributes
/// @p geometry and a path of two open lines, as LibreOffice writes Fontwork.
std::string shape(const std::string &geometry, const std::string &text) {
  return R"(<draw:custom-shape draw:style-name="gr" svg:x="1cm" svg:y="1cm" )"
         R"(svg:width="8cm" svg:height="2cm"><text:p>)" +
         text +
         R"(</text:p><draw:enhanced-geometry svg:viewBox="0 0 21600 21600" )"
         R"(draw:enhanced-path="M 0 0 L 21600 0 N M 0 21600 L 21600 21600 N" )" +
         geometry + "/></draw:custom-shape>";
}

std::optional<TextPathMode> text_path_of(const std::string &path) {
  const odr::Document document = odr::open(path).as_document_file().document();
  return document.root_element()
      .first_child()
      .first_child()
      .as_frame()
      .text_path();
}

std::string render(const std::string &path) {
  std::ostringstream html;
  html::translate(odr::open(path), HtmlConfig())
      .list_views()
      .at(0)
      .write_html(html);
  return html.str();
}

} // namespace

TEST(OdfFontwork, a_text_path_shape_states_its_mode) {
  EXPECT_EQ(
      text_path_of(write_odg(
          "odf_fontwork_shape.odg",
          shape(R"(draw:text-path="true" draw:text-path-mode="shape")", "A"))),
      TextPathMode::shape);
  EXPECT_EQ(
      text_path_of(write_odg(
          "odf_fontwork_path.odg",
          shape(R"(draw:text-path="true" draw:text-path-mode="path")", "A"))),
      TextPathMode::path);
  EXPECT_EQ(text_path_of(write_odg("odf_fontwork_normal.odg",
                                   shape(R"(draw:text-path="true")", "A"))),
            TextPathMode::normal);
  EXPECT_FALSE(text_path_of(write_odg("odf_fontwork_none.odg", shape("", "A")))
                   .has_value());
}

TEST(OdfFontwork, the_text_is_painted_with_the_shape_and_the_path_is_not) {
  const std::string html = render(
      write_odg("odf_fontwork_render.odg",
                shape(R"(draw:text-path="true" draw:text-path-mode="shape")",
                      "Courtesy")));

  EXPECT_EQ(html.find("<path"), std::string::npos);
  const std::size_t text = html.find("<text ");
  ASSERT_NE(text, std::string::npos);
  const std::string element =
      html.substr(text, html.find("</text>", text) - text);
  EXPECT_NE(element.find(R"(lengthAdjust="spacingAndGlyphs")"),
            std::string::npos);
  EXPECT_NE(element.find("fill:#990000"), std::string::npos);
  EXPECT_NE(element.find("stroke:#ffff00"), std::string::npos);
  EXPECT_NE(element.find(">Courtesy"), std::string::npos);
  // the dark sheet keeps it legible, as text
  EXPECT_NE(element.find(R"(class="odr-text-path")"), std::string::npos);
}

TEST(OdfFontwork, explicit_breaks_split_lines_inside_spans) {
  for (const std::string mode : {"normal", "path", "shape"}) {
    const std::string html = render(write_odg(
        "odf_fontwork_breaks.odg",
        shape(R"(draw:text-path="true" draw:text-path-mode=")" + mode + R"(")",
              "<text:span>First<text:line-break/>Second</text:span>")));
    EXPECT_NE(html.find(">First</text>"), std::string::npos);
    EXPECT_NE(html.find(">Second</text>"), std::string::npos);
    EXPECT_EQ(html.find("FirstSecond"), std::string::npos);
  }
}

TEST(OdfFontwork, blank_lines_keep_their_share_of_the_height) {
  const std::string html =
      render(write_odg("odf_fontwork_blank_lines.odg",
                       shape(R"(draw:text-path="true")",
                             "First<text:line-break/><text:line-break/>Last")));
  // 2cm is 75.59055px: the first and third of three equal lines.
  EXPECT_NE(html.find(R"(y="12.59843")"), std::string::npos);
  EXPECT_NE(html.find(R"(y="62.99213")"), std::string::npos);
  EXPECT_NE(html.find(">First</text>"), std::string::npos);
  EXPECT_NE(html.find(">Last</text>"), std::string::npos);
}
