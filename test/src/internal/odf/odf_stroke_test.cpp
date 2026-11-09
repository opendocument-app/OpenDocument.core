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
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

/// A drawing whose content holds @p styles as automatic styles and whose page
/// holds @p body.
std::string write_odg(const std::string &name, const std::string &styles,
                      const std::string &body) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0">)"
      R"(<office:automatic-styles>)" +
      styles +
      R"(</office:automatic-styles>)"
      R"(<office:body><office:drawing><draw:page>)" +
      body +
      R"(</draw:page></office:drawing></office:body>)"
      R"(</office:document-content>)";

  const std::string path = (std::filesystem::current_path() / name).string();

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

std::vector<GraphicStyle> styles_of(const Document &document) {
  std::vector<GraphicStyle> result;
  for (const Element child : document.root_element().first_child().children()) {
    result.push_back(child.as_frame().style());
  }
  return result;
}

std::string rect(const std::string &style) {
  return R"(<draw:rect draw:style-name=")" + style +
         R"(" svg:x="1cm" svg:y="1cm" svg:width="4cm" svg:height="2cm"/>)";
}

} // namespace

TEST(OdfStroke, the_stroke_kind_and_its_color_cascade_independently) {
  const std::string path = write_odg(
      "odf_stroke_cascade.odg",
      R"(<style:style style:name="base" style:family="graphic">)"
      R"(<style:graphic-properties svg:stroke-color="#808080" )"
      R"(svg:stroke-width="0cm"/></style:style>)"
      R"(<style:style style:name="none" style:family="graphic" )"
      R"(style:parent-style-name="base">)"
      R"(<style:graphic-properties draw:stroke="none"/></style:style>)"
      R"(<style:style style:name="again" style:family="graphic" )"
      R"(style:parent-style-name="none">)"
      R"(<style:graphic-properties draw:stroke="solid"/></style:style>)",
      rect("base") + rect("none") + rect("again"));

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).stroke_color.has_value());
  EXPECT_EQ(styles.at(0).stroke_color->alpha, 255);
  ASSERT_TRUE(styles.at(1).stroke_color.has_value());
  EXPECT_EQ(styles.at(1).stroke_color->alpha, 0);
  EXPECT_EQ(styles.at(1).stroke_color->rgb(), 0x808080u);
  ASSERT_TRUE(styles.at(2).stroke_color.has_value());
  EXPECT_EQ(styles.at(2).stroke_color->alpha, 255);
  EXPECT_EQ(styles.at(2).stroke_color->rgb(), 0x808080u);
}

TEST(OdfStroke, stroke_none_with_no_colour_anywhere_draws_no_stroke) {
  const std::string path =
      write_odg("odf_stroke_none.odg",
                R"(<style:style style:name="bare" style:family="graphic">)"
                R"(<style:graphic-properties draw:stroke="none"/>)"
                R"(</style:style>)",
                rect("bare"));

  std::ostringstream html;
  html::translate(odr::open(path), HtmlConfig())
      .list_views()
      .at(0)
      .write_html(html);
  EXPECT_NE(html.str().find("stroke:none"), std::string::npos);
  EXPECT_EQ(html.str().find("stroke:#"), std::string::npos);
}
