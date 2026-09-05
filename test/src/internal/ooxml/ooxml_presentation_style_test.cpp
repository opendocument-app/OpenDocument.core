#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pugixml.hpp>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::ooxml::presentation;

namespace {

using Part = std::pair<std::string, std::string>;

pugi::xml_node node_of(const std::string &xml, pugi::xml_document &document) {
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return document.first_child();
}

std::string slot(const std::string &name, const std::string &value) {
  return "<" + name + R"(><a:srgbClr val=")" + value + R"("/></)" + name + ">";
}

/// The two theme slots the `p:clrMap` cases map from.
const std::string light_and_dark =
    slot("a:dk1", "111111") + slot("a:lt1", "eeeeee");

ColorScheme scheme_of(const std::string &colors, const std::string &map,
                      pugi::xml_document &document) {
  const pugi::xml_node root = node_of("<root><a:clrScheme>" + colors +
                                          "</a:clrScheme>" + map + "</root>",
                                      document);
  return ColorScheme(root.child("a:clrScheme"), root.child("p:clrMap"));
}

std::string slide_like(const std::string &background) {
  return "<p:sld><p:cSld>" + background + "</p:cSld></p:sld>";
}

std::string solid_background(const std::string &fill) {
  return "<p:bg><p:bgPr><a:solidFill>" + fill +
         "</a:solidFill></p:bgPr></p:bg>";
}

constexpr const char *layout_path = "/ppt/slideLayouts/slideLayout1.xml";
constexpr const char *layout_rels_path =
    "/ppt/slideLayouts/_rels/slideLayout1.xml.rels";
constexpr const char *master_path = "/ppt/slideMasters/slideMaster1.xml";
constexpr const char *master_rels_path =
    "/ppt/slideMasters/_rels/slideMaster1.xml.rels";
constexpr const char *theme_path = "/ppt/theme/theme1.xml";

std::string relationship(const std::string &type, const std::string &target) {
  return R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
         R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/)" +
         type + R"(" Target=")" + target + R"("/></Relationships>)";
}

VirtualFilesystem filesystem_of(const std::vector<Part> &parts) {
  VirtualFilesystem result;
  for (const auto &[path, content] : parts) {
    result.copy(std::make_shared<MemoryFile>(content), AbsPath(path));
  }
  return result;
}

/// A layout on a master on a theme, each stating what the case gives it.
std::vector<Part> package(const std::string &layout, const std::string &master,
                          const std::string &theme_colors) {
  return {
      {layout_path, layout},
      {layout_rels_path,
       relationship("slideMaster", "../slideMasters/slideMaster1.xml")},
      {master_path, master},
      {master_rels_path, relationship("theme", "../theme/theme1.xml")},
      {theme_path, "<a:theme><a:themeElements><a:clrScheme>" + theme_colors +
                       "</a:clrScheme></a:themeElements></a:theme>"},
  };
}

/// @p color_schemes owns what the returned style points at, so it outlives it.
LayoutStyle
layout_style_of(const std::vector<Part> &parts,
                std::unordered_map<std::string, ColorScheme> &color_schemes) {
  const VirtualFilesystem filesystem = filesystem_of(parts);
  return load_layout_style(filesystem, AbsPath(layout_path), color_schemes);
}

} // namespace

TEST(ooxml_presentation_style, a_scheme_maps_a_slot_name_onto_a_theme_color) {
  pugi::xml_document document;
  const ColorScheme scheme =
      scheme_of(light_and_dark, R"(<p:clrMap bg1="lt1" tx1="dk1"/>)", document);

  ASSERT_TRUE(scheme.resolve("bg1").has_value());
  EXPECT_EQ(0xffeeeeeeu, scheme.resolve("bg1")->argb());
  ASSERT_TRUE(scheme.resolve("tx1").has_value());
  EXPECT_EQ(0xff111111u, scheme.resolve("tx1")->argb());
  // the theme's own slots stay reachable under their own names
  EXPECT_EQ(0xffeeeeeeu, scheme.resolve("lt1")->argb());
  EXPECT_FALSE(scheme.resolve("accent1").has_value());
}

TEST(ooxml_presentation_style, a_scheme_maps_every_slot_against_the_theme) {
  pugi::xml_document document;
  // each accent is the other's source, so a map written in place would resolve
  // the second against the first's new value
  const ColorScheme scheme =
      scheme_of(slot("a:accent1", "aa0000") + slot("a:accent2", "00bb00"),
                R"(<p:clrMap accent1="accent2" accent2="accent1"/>)", document);

  EXPECT_EQ(0xff00bb00u, scheme.resolve("accent1")->argb());
  EXPECT_EQ(0xffaa0000u, scheme.resolve("accent2")->argb());
}

TEST(ooxml_presentation_style, a_theme_slot_named_without_a_prefix) {
  pugi::xml_document document;
  const ColorScheme scheme = scheme_of(slot("x", "abcdef"), "", document);

  ASSERT_TRUE(scheme.resolve("x").has_value());
  EXPECT_EQ(0xffabcdefu, scheme.resolve("x")->argb());
}

TEST(ooxml_presentation_style, a_system_color_resolves_to_its_last_value) {
  pugi::xml_document document;
  const pugi::xml_node fill = node_of(
      R"(<a:solidFill><a:sysClr val="windowText" lastClr="123456"/></a:solidFill>)",
      document);

  ASSERT_TRUE(read_drawing_color(fill, nullptr).has_value());
  EXPECT_EQ(0xff123456u, read_drawing_color(fill, nullptr)->argb());
}

TEST(ooxml_presentation_style, a_scheme_color_needs_a_scheme_to_resolve) {
  pugi::xml_document scheme_document;
  const ColorScheme scheme =
      scheme_of(light_and_dark, R"(<p:clrMap tx1="dk1"/>)", scheme_document);

  pugi::xml_document document;
  const pugi::xml_node fill = node_of(
      R"(<a:solidFill><a:schemeClr val="tx1"/></a:solidFill>)", document);

  EXPECT_FALSE(read_drawing_color(fill, nullptr).has_value());
  ASSERT_TRUE(read_drawing_color(fill, &scheme).has_value());
  EXPECT_EQ(0xff111111u, read_drawing_color(fill, &scheme)->argb());
}

TEST(ooxml_presentation_style, a_part_that_states_no_ground_inherits_one) {
  pugi::xml_document document;
  EXPECT_FALSE(read_background_color(node_of(slide_like(""), document), nullptr)
                   .has_value());
}

TEST(ooxml_presentation_style, a_ground_we_cannot_read_ends_the_walk) {
  pugi::xml_document document;
  const std::optional<std::optional<Color>> background = read_background_color(
      node_of(slide_like("<p:bg><p:bgPr><a:blipFill/></p:bgPr></p:bg>"),
              document),
      nullptr);

  // it states one, so nothing behind it applies - but we paint nothing
  ASSERT_TRUE(background.has_value());
  EXPECT_FALSE(background->has_value());
}

TEST(ooxml_presentation_style, a_ground_stated_as_a_literal) {
  pugi::xml_document document;
  const std::optional<std::optional<Color>> background = read_background_color(
      node_of(slide_like(solid_background(R"(<a:srgbClr val="fedcba"/>)")),
              document),
      nullptr);

  ASSERT_TRUE(background.has_value());
  ASSERT_TRUE(background->has_value());
  EXPECT_EQ(0xfffedcbau, (*background)->argb());
}

TEST(ooxml_presentation_style, a_layout_takes_its_masters_scheme_and_ground) {
  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle style = layout_style_of(
      package("<p:sldLayout><p:cSld/></p:sldLayout>",
              R"(<p:sldMaster><p:clrMap bg1="lt1" tx1="dk1"/><p:cSld>)" +
                  solid_background(R"(<a:schemeClr val="bg1"/>)") +
                  "</p:cSld></p:sldMaster>",
              light_and_dark),
      color_schemes);

  ASSERT_NE(nullptr, style.color_scheme);
  ASSERT_TRUE(style.color_scheme->resolve("tx1").has_value());
  EXPECT_EQ(0xff111111u, style.color_scheme->resolve("tx1")->argb());
  ASSERT_TRUE(style.background.has_value());
  EXPECT_EQ(0xffeeeeeeu, style.background->argb());
}

TEST(ooxml_presentation_style, a_layouts_own_ground_beats_its_masters) {
  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle style = layout_style_of(
      package("<p:sldLayout><p:cSld>" +
                  solid_background(R"(<a:srgbClr val="00ff00"/>)") +
                  "</p:cSld></p:sldLayout>",
              "<p:sldMaster><p:cSld>" +
                  solid_background(R"(<a:srgbClr val="ff0000"/>)") +
                  "</p:cSld></p:sldMaster>",
              light_and_dark),
      color_schemes);

  ASSERT_TRUE(style.background.has_value());
  EXPECT_EQ(0xff00ff00u, style.background->argb());
}

TEST(ooxml_presentation_style, a_layout_whose_ground_we_cannot_read) {
  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle style = layout_style_of(
      package("<p:sldLayout><p:cSld><p:bg><p:bgPr><a:gradFill/></p:bgPr>"
              "</p:bg></p:cSld></p:sldLayout>",
              "<p:sldMaster><p:cSld>" +
                  solid_background(R"(<a:srgbClr val="ff0000"/>)") +
                  "</p:cSld></p:sldMaster>",
              light_and_dark),
      color_schemes);

  EXPECT_FALSE(style.background.has_value());
}

TEST(ooxml_presentation_style, one_scheme_per_master_however_many_layouts) {
  std::vector<Part> parts =
      package("<p:sldLayout/>", "<p:sldMaster/>", light_and_dark);
  parts.emplace_back("/ppt/slideLayouts/slideLayout2.xml", "<p:sldLayout/>");
  parts.emplace_back(
      "/ppt/slideLayouts/_rels/slideLayout2.xml.rels",
      relationship("slideMaster", "../slideMasters/slideMaster1.xml"));
  const VirtualFilesystem filesystem = filesystem_of(parts);

  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle first =
      load_layout_style(filesystem, AbsPath(layout_path), color_schemes);
  const LayoutStyle second = load_layout_style(
      filesystem, AbsPath("/ppt/slideLayouts/slideLayout2.xml"), color_schemes);

  EXPECT_EQ(1u, color_schemes.size());
  EXPECT_EQ(first.color_scheme, second.color_scheme);
}

TEST(ooxml_presentation_style, a_layout_relating_no_master_stays_unstyled) {
  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle style =
      layout_style_of({{layout_path, "<p:sldLayout/>"}}, color_schemes);

  EXPECT_EQ(nullptr, style.color_scheme);
  EXPECT_FALSE(style.background.has_value());
}

TEST(ooxml_presentation_style, a_layout_part_that_is_not_xml_stays_unstyled) {
  std::unordered_map<std::string, ColorScheme> color_schemes;
  const LayoutStyle style = layout_style_of(
      {{layout_path, "not xml at all"},
       {layout_rels_path,
        relationship("slideMaster", "../slideMasters/slideMaster1.xml")}},
      color_schemes);

  EXPECT_EQ(nullptr, style.color_scheme);
  EXPECT_FALSE(style.background.has_value());
}

TEST(ooxml_presentation_style, a_run_paints_and_highlights_through_the_theme) {
  pugi::xml_document scheme_document;
  const ColorScheme scheme = scheme_of(
      light_and_dark, R"(<p:clrMap bg1="lt1" tx1="dk1"/>)", scheme_document);

  pugi::xml_document document;
  const pugi::xml_node run = node_of(
      R"(<a:r><a:rPr><a:solidFill><a:schemeClr val="tx1"/></a:solidFill>)"
      R"(<a:highlight><a:srgbClr val="ffff00"/></a:highlight></a:rPr></a:r>)",
      document);

  TextStyle style;
  resolve_text_style(run, &scheme, style);

  ASSERT_TRUE(style.font_color.has_value());
  EXPECT_EQ(0xff111111u, style.font_color->argb());
  ASSERT_TRUE(style.background_color.has_value());
  EXPECT_EQ(0xffffff00u, style.background_color->argb());
}

TEST(ooxml_presentation_style, the_baseline_sign_carries_the_direction) {
  pugi::xml_document document;
  TextStyle style;

  resolve_text_style(
      node_of(R"(<a:r><a:rPr baseline="30000"/></a:r>)", document), nullptr,
      style);
  EXPECT_EQ(FontPosition::super, style.font_position);

  pugi::xml_document sub_document;
  resolve_text_style(
      node_of(R"(<a:r><a:rPr baseline="-25000"/></a:r>)", sub_document),
      nullptr, style);
  EXPECT_EQ(FontPosition::sub, style.font_position);

  pugi::xml_document normal_document;
  resolve_text_style(
      node_of(R"(<a:r><a:rPr baseline="0"/></a:r>)", normal_document), nullptr,
      style);
  EXPECT_EQ(FontPosition::normal, style.font_position);
}

TEST(ooxml_presentation_style, line_spacing_is_a_percent_or_a_length) {
  pugi::xml_document percent_document;
  ParagraphStyle percent;
  resolve_paragraph_style(
      node_of(
          R"(<a:p><a:pPr><a:lnSpc><a:spcPct val="150000"/></a:lnSpc></a:pPr></a:p>)",
          percent_document),
      percent);
  ASSERT_TRUE(percent.line_height.has_value());
  EXPECT_EQ(Measure(150, DynamicUnit("%")), *percent.line_height);

  pugi::xml_document points_document;
  ParagraphStyle points;
  resolve_paragraph_style(
      node_of(
          R"(<a:p><a:pPr><a:lnSpc><a:spcPts val="1800"/></a:lnSpc></a:pPr></a:p>)",
          points_document),
      points);
  ASSERT_TRUE(points.line_height.has_value());
  EXPECT_EQ(Measure(18, DynamicUnit("pt")), *points.line_height);
}

/// [ECMA-376] 21.1.2.2.7. Absent says nothing, `0` says left-to-right.
TEST(ooxml_presentation_style, rtl_reads_as_a_paragraph_direction) {
  const auto direction_of = [](const char *xml) {
    pugi::xml_document document;
    ParagraphStyle style;
    resolve_paragraph_style(node_of(xml, document), style);
    return style.direction;
  };

  EXPECT_EQ(TextDirection::right_to_left,
            direction_of(R"(<a:p><a:pPr rtl="1"/></a:p>)"));
  EXPECT_EQ(TextDirection::left_to_right,
            direction_of(R"(<a:p><a:pPr rtl="0"/></a:p>)"));
  EXPECT_FALSE(direction_of(R"(<a:p><a:pPr/></a:p>)").has_value());
}

TEST(ooxml_presentation_style, paragraph_spacing_is_taken_absolute_only) {
  pugi::xml_document document;
  ParagraphStyle style;
  resolve_paragraph_style(
      node_of(R"(<a:p><a:pPr><a:spcBef><a:spcPts val="600"/></a:spcBef>)"
              R"(<a:spcAft><a:spcPct val="20000"/></a:spcAft></a:pPr></a:p>)",
              document),
      style);

  ASSERT_TRUE(style.margin.top.has_value());
  EXPECT_EQ(Measure(6, DynamicUnit("pt")), *style.margin.top);
  EXPECT_FALSE(style.margin.bottom.has_value());
}
