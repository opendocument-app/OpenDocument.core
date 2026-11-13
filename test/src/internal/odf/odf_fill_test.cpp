#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>

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

constexpr const char *namespaces =
    R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
    R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
    R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
    R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
    R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")";

/// A drawing whose `styles.xml` holds @p styles under `office:styles` and
/// whose page holds @p body; with @p generator, a `meta.xml` naming it, and
/// stating ODF @p version, none where it is empty.
std::string write_odg(const std::string &name, const std::string &styles,
                      const std::string &body,
                      const std::string &generator = {},
                      const std::string &version = "1.3") {
  const std::string content =
      std::string(R"(<?xml version="1.0" encoding="UTF-8"?>)") +
      "<office:document-content " + namespaces +
      (version.empty() ? std::string()
                       : R"( office:version=")" + version + R"(")") +
      ">" + R"(<office:body><office:drawing><draw:page>)" + body +
      R"(</draw:page></office:drawing></office:body>)"
      R"(</office:document-content>)";
  const std::string styles_xml =
      std::string(R"(<?xml version="1.0" encoding="UTF-8"?>)") +
      "<office:document-styles " + namespaces + "><office:styles>" + styles +
      "</office:styles></office:document-styles>";

  const std::string path =
      (std::filesystem::temp_directory_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.graphics"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles_xml));
  if (!generator.empty()) {
    zip.insert_file(
        std::end(zip), RelPath("meta.xml"),
        std::make_shared<MemoryFile>(
            R"(<?xml version="1.0" encoding="UTF-8"?><office:document-meta )"
            R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
            R"(xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0">)"
            R"(<office:meta><meta:generator>)" +
            generator +
            R"(</meta:generator></office:meta></office:document-meta>)"));
  }
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

std::vector<GraphicStyle> styles_of(const odr::Document &document) {
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

/// The html of the first view of the file at @p path.
std::string render(const std::string &path,
                   const HtmlConfig &config = HtmlConfig()) {
  std::ostringstream html;
  html::translate(odr::open(path), config).list_views().at(0).write_html(html);
  return html.str();
}

} // namespace

TEST(OdfFill, a_gradient_is_read_with_its_intensities_and_its_angle) {
  const std::string path = write_odg(
      "odf_fill_gradient.odg",
      R"(<draw:gradient draw:name="G" draw:style="axial" )"
      R"(draw:start-color="#ff0000" draw:end-color="#0000ff" )"
      R"(draw:start-intensity="50%" draw:end-intensity="100%" )"
      R"(draw:angle="300" draw:border="20%" draw:cx="10%" draw:cy="90%"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="G"/></style:style>)",
      rect("g"),
      "LibreOffice/6.4.7.2$Linux_X86_64 LibreOffice_project/40$Build-2", "1.2");

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_gradient.has_value());
  EXPECT_FALSE(style.fill_hatch.has_value());
  const GradientFill &gradient = *style.fill_gradient;
  EXPECT_EQ(gradient.style, GradientStyle::axial);
  EXPECT_EQ(gradient.start_color.rgb(), 0x800000u);
  EXPECT_EQ(gradient.end_color.rgb(), 0x0000ffu);
  EXPECT_DOUBLE_EQ(gradient.angle, 30);
  EXPECT_DOUBLE_EQ(gradient.border, 0.2);
  EXPECT_DOUBLE_EQ(gradient.center_x, 0.1);
  EXPECT_DOUBLE_EQ(gradient.center_y, 0.9);
}

TEST(OdfFill, a_gradient_angle_with_a_unit_is_read_in_that_unit) {
  const std::string path =
      write_odg("odf_fill_gradient_angle.odg",
                R"(<draw:gradient draw:name="D" draw:angle="27deg"/>)"
                R"(<draw:gradient draw:name="G" draw:angle="100grad"/>)"
                R"(<style:style style:name="d" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="D"/></style:style>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                rect("d") + rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  EXPECT_DOUBLE_EQ(styles.at(0).fill_gradient.value().angle, 27);
  EXPECT_DOUBLE_EQ(styles.at(1).fill_gradient.value().angle, 90);
}

TEST(OdfFill, the_fill_kind_and_the_gradient_name_cascade_independently) {
  const std::string path = write_odg(
      "odf_fill_cascade.odg",
      R"(<draw:gradient draw:name="G" draw:style="linear"/>)"
      R"(<style:style style:name="base" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill-gradient-name="G"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="gradient" style:family="graphic" )"
      R"(style:parent-style-name="base">)"
      R"(<style:graphic-properties draw:fill="gradient"/></style:style>)"
      R"(<style:style style:name="solid" style:family="graphic" )"
      R"(style:parent-style-name="gradient">)"
      R"(<style:graphic-properties draw:fill="solid" )"
      R"(draw:fill-color="#00ff00"/></style:style>)",
      rect("gradient") + rect("solid"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  EXPECT_TRUE(styles.at(0).fill_gradient.has_value());
  EXPECT_FALSE(styles.at(1).fill_gradient.has_value());
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 255);
}

TEST(OdfFill, a_solid_hatch_paints_its_fill_color_behind_the_lines) {
  const std::string path = write_odg(
      "odf_fill_hatch.odg",
      R"(<draw:hatch draw:name="H" draw:style="double" draw:color="#3465a4" )"
      R"(draw:distance="0.15cm" draw:rotation="450"/>)"
      R"(<style:style style:name="solid" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-hatch-solid="true" draw:fill-color="#009eda"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="lines" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-color="#009eda"/></style:style>)",
      rect("solid") + rect("lines"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).fill_hatch.has_value());
  const HatchFill &hatch = *styles.at(0).fill_hatch;
  EXPECT_EQ(hatch.style, HatchStyle::crossed);
  EXPECT_EQ(hatch.color.rgb(), 0x3465a4u);
  EXPECT_EQ(hatch.distance.to_string(), "0.15cm");
  EXPECT_DOUBLE_EQ(hatch.rotation, 45);
  ASSERT_TRUE(styles.at(0).fill_color.has_value());
  EXPECT_EQ(styles.at(0).fill_color->alpha, 255);

  ASSERT_TRUE(styles.at(1).fill_hatch.has_value());
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 0);
}

TEST(OdfFill, a_gradient_that_names_nothing_paints_nothing) {
  const std::string path = write_odg(
      "odf_fill_missing.odg",
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="missing" draw:fill-color="#00ff00"/>)"
      R"(</style:style>)",
      rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  EXPECT_FALSE(style.fill_gradient.has_value());
  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->alpha, 0);
}

TEST(OdfFill,
     a_unitless_gradient_angle_is_in_degrees_unless_its_producer_wrote_tenths) {
  const std::string styles =
      R"(<draw:gradient draw:name="G" draw:angle="300"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="G"/></style:style>)";
  const auto angle = [&](const std::string &name, const std::string &generator,
                         const std::string &version) {
    const odr::Document document =
        odr::open(write_odg(name, styles, rect("g"), generator, version))
            .as_document_file()
            .document();
    return styles_of(document).at(0).fill_gradient.value().angle;
  };
  const std::string lo6 =
      "LibreOffice/6.4.7.2$Linux_X86_64 LibreOffice_project/40$Build-2";
  const std::string lo7 =
      "LibreOffice/7.3.7.2$Linux_X86_64 LibreOffice_project/30$Build-2";

  // at ODF 1.2 the producer decides
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_unknown.odg", "", "1.2"), 300);
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_lo7.odg", lo7, "1.2"), 300);
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_lo6.odg", lo6, "1.2"), 30);
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_collabora.odg",
                         "CollaboraOffice/6.4.10.4$Linux_X86_64 "
                         "LibreOffice_project/fb2$Build-4",
                         "1.2"),
                   30);
  EXPECT_DOUBLE_EQ(
      angle("odf_fill_angle_aoo.odg", "Apache_OpenOffice/4.1.15$Win32", "1.2"),
      30);
  // below it tenths, as an Excel ods stating no version writes them
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_none.odg", "", ""), 30);
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_1_1.odg", lo7, "1.1"), 30);
  // above it degrees, whoever wrote it
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_1_3.odg", lo6, "1.3"), 300);
}

TEST(OdfFill, a_gradient_stating_nothing_is_black_with_its_center_at_zero) {
  // as LibreOffice's XMLGradientStyleImport starts from
  const std::string path =
      write_odg("odf_fill_gradient_bare.odg",
                R"(<draw:gradient draw:name="G" draw:style="radial"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_gradient.has_value());
  EXPECT_EQ(style.fill_gradient->start_color.rgb(), 0x000000u);
  EXPECT_EQ(style.fill_gradient->end_color.rgb(), 0x000000u);
  EXPECT_DOUBLE_EQ(style.fill_gradient->center_x, 0);
  EXPECT_DOUBLE_EQ(style.fill_gradient->center_y, 0);
}

TEST(OdfFill, a_hatch_without_a_distance_is_drawn_at_the_minimum) {
  const std::string path = write_odg(
      "odf_fill_hatch_zero.odg",
      R"(<draw:hatch draw:name="H" draw:style="single" draw:color="#000000")"
      R"( draw:distance="0cm"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" )"
      R"(draw:fill-hatch-name="H"/></style:style>)",
      rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  ASSERT_TRUE(styles_of(document).at(0).fill_hatch.has_value());

  // the lines 2px apart, percent-encoded in the pattern
  EXPECT_NE(render(path).find("width=%222px%22"), std::string::npos);
}

TEST(OdfFill, a_shape_paints_its_gradient_on_a_layer_masked_to_it) {
  const std::string path =
      write_odg("odf_fill_html.odg",
                R"(<draw:gradient draw:name="G" draw:style="linear" )"
                R"(draw:start-color="#ff0000" draw:end-color="#0000ff"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                R"(<draw:ellipse draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                R"(svg:width="4cm" svg:height="2cm"/>)"
                R"(<draw:path draw:style-name="g" svg:x="1cm" svg:y="4cm" )"
                R"(svg:width="4cm" svg:height="2cm" svg:viewBox="0 0 400 200" )"
                R"(svg:d="M0 0 L400 200"/>)");

  const std::string page = render(path);

  // one layer, for the closed ellipse; the open path is not filled
  const std::size_t layer = page.find(R"(class="odr-fill")");
  ASSERT_NE(layer, std::string::npos);
  EXPECT_EQ(page.find(R"(class="odr-fill")", layer + 1), std::string::npos);
  EXPECT_NE(page.find("linear-gradient(180deg,#ff0000 0%,#0000ff 100%)"),
            std::string::npos);
  EXPECT_NE(page.find("mask-image:url('data:image/svg+xml,"),
            std::string::npos);
  // the shape's svg only strokes
  EXPECT_NE(page.find("fill:none;"), std::string::npos);
}

TEST(OdfFill, a_solid_hatch_with_no_color_paints_libreoffices_default) {
  const std::string path = write_odg(
      "odf_fill_hatch_no_color.odg",
      R"(<draw:hatch draw:name="H" draw:color="#000000" )"
      R"(draw:distance="0.1cm"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-hatch-solid="true"/></style:style>)",
      rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->rgb(), 0x729fcfu);
  EXPECT_EQ(style.fill_color->alpha, 255);
}

TEST(OdfFill, a_malformed_meta_xml_names_no_producer) {
  const std::string path =
      write_odg("odf_fill_meta_broken.odg",
                R"(<draw:gradient draw:name="G" draw:angle="300"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                rect("g"), "<broken", "1.2");

  // past the open, which reads `meta.xml` for its statistics
  const odr::Document document(std::make_shared<odf::Document>(
      FileType::opendocument_graphics, DocumentType::drawing,
      zip::ZipFile(std::make_shared<DiskFile>(path)).archive()->as_filesystem(),
      EncryptionState::not_encrypted));
  EXPECT_DOUBLE_EQ(styles_of(document).at(0).fill_gradient.value().angle, 300);
}

TEST(OdfShadow, a_visible_shadow_is_read_with_its_offsets_and_opacity) {
  const std::string path = write_odg(
      "odf_fill_shadow.odg",
      R"(<style:style style:name="base" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow-offset-x="0cm" )"
      R"(draw:shadow-offset-y="-0.07cm" draw:shadow-color="#f49100" )"
      R"(draw:shadow-opacity="50%"/></style:style>)"
      R"(<style:style style:name="shadow" style:family="graphic" )"
      R"(style:parent-style-name="base">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)"
      R"(<style:style style:name="hidden" style:family="graphic" )"
      R"(style:parent-style-name="shadow">)"
      R"(<style:graphic-properties draw:shadow="hidden"/></style:style>)",
      rect("shadow") + rect("hidden"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).shadow.has_value());
  const Shadow &shadow = *styles.at(0).shadow;
  EXPECT_EQ(shadow.offset_x.to_string(), "0cm");
  EXPECT_EQ(shadow.offset_y.to_string(), "-0.07cm");
  EXPECT_EQ(shadow.color.rgb(), 0xf49100u);
  EXPECT_EQ(shadow.color.alpha, 128);
  EXPECT_FALSE(styles.at(1).shadow.has_value());
}

TEST(OdfShadow, a_visible_shadow_stating_nothing_else_is_black_and_unshifted) {
  const std::string path = write_odg(
      "odf_fill_shadow_frame.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)",
      R"(<draw:frame draw:style-name="s" svg:x="1cm" svg:y="1cm" )"
      R"(svg:width="4cm" svg:height="2cm"><draw:text-box>)"
      R"(<text:p>A</text:p></draw:text-box></draw:frame>)");

  const odr::Document document = odr::open(path).as_document_file().document();
  const Frame frame =
      document.root_element().first_child().first_child().as_frame();

  ASSERT_TRUE(frame.style().shadow.has_value());
  EXPECT_EQ(frame.style().shadow->offset_x.to_string(), "0cm");
  EXPECT_EQ(frame.style().shadow->color.rgb(), 0x000000u);
}

TEST(OdfShadow, a_shape_with_a_fill_layer_casts_one_shadow) {
  const std::string path =
      write_odg("odf_fill_shadow_layer.odg",
                R"(<draw:gradient draw:name="G" draw:style="linear" )"
                R"(draw:start-color="#ff0000" draw:end-color="#0000ff"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G" draw:shadow="visible" )"
                R"(draw:shadow-offset-x="0.1cm"/></style:style>)",
                R"(<draw:ellipse draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                R"(svg:width="4cm" svg:height="2cm"/>)"
                R"(<draw:path draw:style-name="g" svg:x="1cm" svg:y="4cm" )"
                R"(svg:width="4cm" svg:height="2cm" svg:viewBox="0 0 400 200" )"
                R"(svg:d="M0 0 L400 200"/>)");

  const std::string page = render(path);

  // from a box around the layer and the svg, which the layer's mask does not
  // clip; none on the shape's own box
  EXPECT_EQ(page.find("box-shadow", page.find("<body")), std::string::npos);
  const std::string shadow = "filter:drop-shadow(0.1cm 0cm 0cm #000000)";
  std::size_t shadows = 0;
  for (std::size_t at = page.find(shadow, page.find("<body"));
       at != std::string::npos; at = page.find(shadow, at + 1)) {
    ++shadows;
  }
  EXPECT_EQ(shadows, 2U);
  const std::size_t layer = page.find(R"(class="odr-fill")");
  ASSERT_NE(layer, std::string::npos);
  const std::size_t layer_end = page.find('>', layer);
  EXPECT_EQ(page.substr(layer, layer_end - layer).find("drop-shadow"),
            std::string::npos);
}

TEST(OdfShadow, a_shape_casting_no_shadow_opens_no_box_around_its_layer) {
  const std::string path =
      write_odg("odf_fill_shadow_none.odg",
                R"(<draw:gradient draw:name="G" draw:style="linear" )"
                R"(draw:start-color="#ff0000" draw:end-color="#0000ff"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                R"(<draw:ellipse draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                R"(svg:width="4cm" svg:height="2cm"/>)");

  const std::string page = render(path);

  ASSERT_NE(page.find(R"(class="odr-fill")"), std::string::npos);
  EXPECT_EQ(page.find("<div style=\"z-index:-1;"), std::string::npos);
}

TEST(OdfShadow, what_a_frame_holds_does_not_cast_its_shadow) {
  const std::string path = write_odg(
      "odf_fill_shadow_held.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)"
      R"(<style:style style:name="h" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="hidden"/></style:style>)",
      R"(<draw:frame draw:style-name="s" svg:width="8cm" svg:height="4cm">)"
      R"(<draw:text-box><text:p>)" +
          rect("h") +
          R"(<draw:rect svg:width="1cm" svg:height="1cm"/>)"
          R"(</text:p></draw:text-box></draw:frame>)");

  const odr::Document document = odr::open(path).as_document_file().document();
  const Element frame = document.root_element().first_child().first_child();
  std::vector<GraphicStyle> held;
  // the shapes in the paragraph of the frame's text box
  for (const Element child : frame.first_child().first_child().children()) {
    held.push_back(child.as_frame().style());
  }

  ASSERT_TRUE(frame.as_frame().style().shadow.has_value());
  ASSERT_EQ(held.size(), 2U);
  EXPECT_FALSE(held.at(0).shadow.has_value());
  EXPECT_FALSE(held.at(1).shadow.has_value());
}

TEST(OdfShadow, a_shadow_cascades_through_the_parent_and_default_styles) {
  const std::string path = write_odg(
      "odf_fill_shadow_cascade.odg",
      R"(<style:default-style style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible" )"
      R"(draw:shadow-offset-x="0.3cm"/></style:default-style>)"
      R"(<style:style style:name="p" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible" )"
      R"(draw:shadow-offset-x="0.2cm"/></style:style>)"
      R"(<style:style style:name="c" style:family="graphic" )"
      R"(style:parent-style-name="p">)"
      R"(<style:graphic-properties draw:fill="none"/></style:style>)"
      R"(<style:style style:name="e" style:family="graphic" )"
      R"(style:parent-style-name="p"/>)"
      R"(<style:style style:name="h" style:family="graphic" )"
      R"(style:parent-style-name="p">)"
      R"(<style:graphic-properties draw:shadow="hidden"/></style:style>)"
      R"(<style:style style:name="d" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="none"/></style:style>)",
      rect("c") + rect("e") + rect("h") + rect("d"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).shadow.has_value());
  EXPECT_EQ(styles.at(0).shadow->offset_x.to_string(), "0.2cm");
  ASSERT_TRUE(styles.at(1).shadow.has_value());
  EXPECT_EQ(styles.at(1).shadow->offset_x.to_string(), "0.2cm");
  EXPECT_FALSE(styles.at(2).shadow.has_value());
  ASSERT_TRUE(styles.at(3).shadow.has_value());
  EXPECT_EQ(styles.at(3).shadow->offset_x.to_string(), "0.3cm");
}

TEST(OdfShadow, a_group_casts_no_shadow_but_its_children_cast_theirs) {
  const std::string path = write_odg(
      "odf_fill_shadow_group.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)"
      R"(<style:style style:name="n" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid"/></style:style>)"
      R"(<style:style style:name="o" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid" draw:shadow="visible" )"
      R"(draw:shadow-offset-x="0.4cm"/></style:style>)",
      R"(<draw:g draw:style-name="s">)" + rect("n") + rect("o") +
          R"(</draw:g>)");

  const odr::Document document = odr::open(path).as_document_file().document();
  const Element group = document.root_element().first_child().first_child();
  const Element own = group.first_child().next_sibling();

  EXPECT_FALSE(group.as_frame().style().shadow.has_value());
  EXPECT_FALSE(group.first_child().as_frame().style().shadow.has_value());
  ASSERT_TRUE(own.as_frame().style().shadow.has_value());
  EXPECT_EQ(own.as_frame().style().shadow->offset_x.to_string(), "0.4cm");
}

TEST(OdfShadow, a_negative_blur_is_none) {
  const std::string path =
      write_odg("odf_fill_shadow_blur.odg",
                R"(<style:style style:name="s" style:family="graphic">)"
                R"(<style:graphic-properties draw:shadow="visible" )"
                R"(loext:shadow-blur="-0.2cm"/></style:style>)",
                rect("s"));

  const odr::Document document = odr::open(path).as_document_file().document();

  EXPECT_EQ(styles_of(document).at(0).shadow.value().blur.to_string(), "0cm");
  EXPECT_NE(render(path).find("drop-shadow(0cm 0cm 0cm #000000)"),
            std::string::npos);
}

TEST(OdfShadow, a_frame_that_paints_no_area_casts_the_shadow_of_its_text) {
  const std::string path = write_odg(
      "odf_fill_shadow_text.odg",
      R"(<style:style style:name="t" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="none" draw:shadow="visible" )"
      R"(draw:shadow-offset-x="0.1cm"/></style:style>)"
      R"(<style:style style:name="f" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid" draw:fill-color="#ffffff" )"
      R"(draw:shadow="visible" draw:shadow-offset-x="0.2cm"/></style:style>)",
      R"(<draw:frame draw:style-name="t" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>A</text:p></draw:text-box></draw:frame>)"
      R"(<draw:frame draw:style-name="f" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>B</text:p></draw:text-box></draw:frame>)");

  const std::string page = render(path);

  EXPECT_NE(page.find("filter:drop-shadow(0.1cm 0cm 0cm #000000);"),
            std::string::npos);
  EXPECT_EQ(page.find("box-shadow:0.1cm"), std::string::npos);
  EXPECT_NE(page.find("box-shadow:0.2cm 0cm 0cm #000000;"), std::string::npos);
}

TEST(OdfShadow, a_fontwork_shape_casts_the_shadow_of_its_text) {
  const std::string path = write_odg(
      "odf_fill_shadow_fontwork.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid" draw:shadow="visible" )"
      R"(draw:shadow-offset-x="0.1cm"/></style:style>)",
      R"(<draw:custom-shape draw:style-name="s" svg:width="8cm" )"
      R"(svg:height="2cm"><text:p>Fontwork</text:p>)"
      R"(<draw:enhanced-geometry svg:viewBox="0 0 21600 21600" )"
      R"(draw:enhanced-path="M 0 0 L 21600 0 N" draw:text-path="true"/>)"
      R"(</draw:custom-shape>)");

  const std::string page = render(path);

  const std::size_t text = page.find(R"(class="odr-text-path")");
  ASSERT_NE(text, std::string::npos);
  const std::size_t svg = page.rfind("<svg", text);
  ASSERT_NE(svg, std::string::npos);
  EXPECT_NE(page.substr(svg, text - svg)
                .find("filter:drop-shadow(0.1cm 0cm 0cm #000000);"),
            std::string::npos);
}

TEST(OdfShadow, the_dark_sheet_clears_every_shadow) {
  const std::string path = write_odg(
      "odf_fill_shadow_dark.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid" draw:shadow="visible"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="t" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="none" draw:shadow="visible"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="n" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="solid"/></style:style>)",
      R"(<draw:frame draw:style-name="s" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>A</text:p></draw:text-box></draw:frame>)"
      R"(<draw:frame draw:style-name="t" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>B</text:p></draw:text-box></draw:frame>)"
      R"(<draw:line draw:style-name="s" svg:x1="0cm" svg:y1="0cm" )"
      R"(svg:x2="4cm" svg:y2="2cm"/>)" +
          rect("s") + rect("n"));
  HtmlConfig config;
  config.color_scheme = HtmlColorScheme::dark;

  const std::string page = render(path, config);

  std::size_t marked = 0;
  for (std::size_t at = page.find(R"(class="odr-shadow")");
       at != std::string::npos;
       at = page.find(R"(class="odr-shadow")", at + 1)) {
    ++marked;
  }
  // a box with and without a fill, a line, a shape
  EXPECT_EQ(marked, 4U);
  EXPECT_NE(page.find(".odr-shadow,.odr-shadow>*{box-shadow:none!important;"
                      "filter:none!important}"),
            std::string::npos);
}

TEST(OdfShadow, a_frame_with_a_bare_hatch_casts_the_shadow_of_its_lines) {
  const std::string path = write_odg(
      "odf_fill_shadow_hatch.odg",
      R"(<draw:hatch draw:name="H" draw:style="single" draw:color="#000000" )"
      R"(draw:distance="0.2cm" draw:rotation="0"/>)"
      R"(<style:style style:name="h" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:shadow="visible" draw:shadow-offset-x="0.1cm"/></style:style>)"
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-hatch-solid="true" draw:fill-color="#ffffff" )"
      R"(draw:shadow="visible" draw:shadow-offset-x="0.2cm"/></style:style>)",
      R"(<draw:frame draw:style-name="h" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>A</text:p></draw:text-box></draw:frame>)"
      R"(<draw:frame draw:style-name="s" svg:width="4cm" svg:height="2cm">)"
      R"(<draw:text-box><text:p>B</text:p></draw:text-box></draw:frame>)");

  const std::string page = render(path);

  // the bare hatch paints only its lines; on a solid ground it covers the box
  EXPECT_NE(page.find("filter:drop-shadow(0.1cm 0cm 0cm #000000);"),
            std::string::npos);
  EXPECT_EQ(page.find("box-shadow:0.1cm"), std::string::npos);
  EXPECT_NE(page.find("box-shadow:0.2cm 0cm 0cm #000000;"), std::string::npos);
}

TEST(OdfDefaultFill, a_shape_stating_no_fill_is_filled_and_a_frame_is_not) {
  // As LibreOffice writes a Writer header's shape: a fill color, no
  // `draw:fill`, the solid default left unwritten.
  const std::string path = write_odg(
      "odf_fill_default.odg",
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill-color="#fffc00"/>)"
      R"(</style:style>)",
      rect("g") + R"(<draw:frame draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                  R"(svg:width="4cm" svg:height="2cm"/>)");

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).fill_color.has_value());
  EXPECT_EQ(styles.at(0).fill_color->alpha, 255);
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 0);
}

TEST(OdfDefaultFill, a_shape_stating_no_fill_and_no_color_takes_libreoffices) {
  const std::string path =
      write_odg("odf_fill_default_color.odg",
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties svg:stroke-color="#000000"/>)"
                R"(</style:style>)",
                rect("g"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->rgb(), 0x729fcfu);
  EXPECT_EQ(style.fill_color->alpha, 255);
}

TEST(OdfDefaultFill, a_shape_filled_solid_with_no_color_takes_libreoffices) {
  // `draw:fill` stated, `draw:fill-color` nowhere in the chain
  const std::string path =
      write_odg("odf_fill_solid_no_color.odg",
                R"(<style:style style:name="solid" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="solid"/></style:style>)"
                R"(<style:style style:name="none" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="none"/></style:style>)",
                rect("solid") + rect("none"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).fill_color.has_value());
  EXPECT_EQ(styles.at(0).fill_color->rgb(), 0x729fcfu);
  EXPECT_EQ(styles.at(0).fill_color->alpha, 255);
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 0);
}

TEST(OdfDefaultFill, a_fill_stated_above_the_shape_is_kept) {
  // the group's style is part of the shape's, as `get_intermediate_style`
  // reads it
  const std::string path = write_odg(
      "odf_fill_default_group.odg",
      R"(<style:style style:name="none" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="none"/></style:style>)"
      R"(<style:style style:name="plain" style:family="graphic">)"
      R"(<style:graphic-properties svg:stroke-color="#000000"/>)"
      R"(</style:style>)",
      R"(<draw:g draw:style-name="none">)" + rect("plain") + "</draw:g>");

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = document.root_element()
                                 .first_child()
                                 .first_child()
                                 .first_child()
                                 .as_frame()
                                 .style();

  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->alpha, 0);
}

TEST(OdfDefaultFill, a_default_style_stating_a_fill_is_kept) {
  const std::string path = write_odg(
      "odf_fill_default_style.odg",
      R"(<style:default-style style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="none"/></style:default-style>)"
      R"(<style:style style:name="plain" style:family="graphic">)"
      R"(<style:graphic-properties svg:stroke-color="#000000"/>)"
      R"(</style:style>)",
      rect("plain"));

  const odr::Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->alpha, 0);
}

TEST(OdfDefaultFill, a_shape_enclosing_no_area_takes_no_default_fill) {
  // A line, a measure, a polyline and a connector show no fill, so the
  // default would only add markup; a polygon encloses its area and keeps it.
  const std::string path = write_odg(
      "odf_fill_default_lines.odg",
      R"(<style:style style:name="plain" style:family="graphic">)"
      R"(<style:graphic-properties svg:stroke-color="#000000"/>)"
      R"(</style:style>)",
      R"(<draw:line draw:style-name="plain" svg:x1="1cm" svg:y1="1cm" )"
      R"(svg:x2="5cm" svg:y2="1cm"/>)"
      R"(<draw:measure draw:style-name="plain" svg:x1="1cm" svg:y1="2cm" )"
      R"(svg:x2="5cm" svg:y2="2cm"/>)"
      R"(<draw:polyline draw:style-name="plain" svg:x="1cm" svg:y="3cm" )"
      R"(svg:width="4cm" svg:height="2cm" svg:viewBox="0 0 4000 2000" )"
      R"(draw:points="0,0 2000,2000 4000,0"/>)"
      R"(<draw:connector draw:style-name="plain" svg:x1="1cm" svg:y1="6cm" )"
      R"(svg:x2="5cm" svg:y2="7cm"/>)"
      R"(<draw:polygon draw:style-name="plain" svg:x="1cm" svg:y="8cm" )"
      R"(svg:width="4cm" svg:height="2cm" svg:viewBox="0 0 4000 2000" )"
      R"(draw:points="0,0 2000,2000 4000,0"/>)");

  const odr::Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_EQ(styles.size(), 5);
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_TRUE(!styles.at(i).fill_color.has_value() ||
                styles.at(i).fill_color->alpha == 0)
        << "shape " << i;
  }
  ASSERT_TRUE(styles.at(4).fill_color.has_value());
  EXPECT_EQ(styles.at(4).fill_color->rgb(), 0x729fcfu);
  EXPECT_EQ(styles.at(4).fill_color->alpha, 255);
}

TEST(OdfDefaultFill, an_open_path_shows_no_fill) {
  const std::string path = write_odg(
      "odf_fill_default_paths.odg", "",
      R"(<draw:path svg:x="1cm" svg:y="1cm" svg:width="4cm" svg:height="2cm" )"
      R"(svg:viewBox="0 0 4000 2000" svg:d="M 0 0 L 2000 2000 4000 0"/>)"
      R"(<draw:path svg:x="1cm" svg:y="4cm" svg:width="4cm" svg:height="2cm" )"
      R"(svg:viewBox="0 0 4000 2000" svg:d="M 0 0 L 2000 2000 4000 0 Z"/>)");

  const std::string html = render(path);
  const std::size_t open = html.find("<path");
  ASSERT_NE(open, std::string::npos);
  const std::size_t closed = html.find("<path", open + 1);
  ASSERT_NE(closed, std::string::npos);

  EXPECT_NE(html.substr(open, closed - open).find(R"(fill="none")"),
            std::string::npos);
  EXPECT_EQ(html.substr(closed, html.find('>', closed) - closed)
                .find(R"(fill="none")"),
            std::string::npos);
}
