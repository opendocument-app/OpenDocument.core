#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <pugixml.hpp>

using namespace odr;
using namespace odr::internal::ooxml::text;

namespace {

/// `count` styles, `s0` based on `s1` and so on; only the last names a font
/// size, so reading it off `s0` proves the whole chain resolved.
std::string based_on_chain(const std::size_t count) {
  std::string result = "<w:styles>";
  for (std::size_t i = 0; i < count; ++i) {
    result += R"(<w:style w:styleId="s)" + std::to_string(i) + R"(">)";
    if (i + 1 < count) {
      result += R"(<w:basedOn w:val="s)" + std::to_string(i + 1) + R"("/>)";
    } else {
      result += R"(<w:rPr><w:sz w:val="48"/></w:rPr>)";
    }
    result += "</w:style>";
  }
  result += "</w:styles>";
  return result;
}

StyleRegistry registry_of(const std::string &xml,
                          pugi::xml_document &document) {
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return StyleRegistry(document.child("w:styles"));
}

pugi::xml_node node_of(const char *xml, pugi::xml_document &document) {
  EXPECT_TRUE(document.load_string(xml));
  return document.first_child();
}

} // namespace

TEST(ooxml_text_style, based_on_chain_inherits) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(based_on_chain(3), document);

  const Style *style = registry.style("s0");
  ASSERT_NE(nullptr, style);
  ASSERT_NE(nullptr, style->parent());
  EXPECT_EQ("s1", style->parent()->name());
  EXPECT_EQ("s2", style->parent()->parent()->name());
  EXPECT_EQ(nullptr, style->parent()->parent()->parent());

  ASSERT_TRUE(style->resolved().text_style.font_size.has_value());
  EXPECT_EQ(Measure(24, DynamicUnit("pt")),
            *style->resolved().text_style.font_size);
}

/// On a thread, whose stack is the small one an http worker gets.
TEST(ooxml_text_style, deep_based_on_chain_resolves) {
  constexpr std::size_t count = 100000;

  std::optional<Measure> font_size;
  bool last_is_root = false;

  std::thread worker([&font_size, &last_is_root] {
    pugi::xml_document document;
    const StyleRegistry registry = registry_of(based_on_chain(count), document);

    if (const Style *style = registry.style("s0"); style != nullptr) {
      font_size = style->resolved().text_style.font_size;
    }
    if (const Style *last = registry.style("s" + std::to_string(count - 1));
        last != nullptr) {
      last_is_root = last->parent() == nullptr;
    }
  });
  worker.join();

  ASSERT_TRUE(font_size.has_value());
  EXPECT_EQ(Measure(24, DynamicUnit("pt")), *font_size);
  EXPECT_TRUE(last_is_root);
}

/// A `w:basedOn` cycle resolves to styles that exist and end somewhere.
TEST(ooxml_text_style, cyclic_based_on_chain_terminates) {
  pugi::xml_document document;
  const StyleRegistry registry =
      registry_of(R"(<w:styles>)"
                  R"(<w:style w:styleId="a"><w:basedOn w:val="b"/></w:style>)"
                  R"(<w:style w:styleId="b"><w:basedOn w:val="c"/></w:style>)"
                  R"(<w:style w:styleId="c"><w:basedOn w:val="a"/></w:style>)"
                  R"(</w:styles>)",
                  document);

  for (const char *name : {"a", "b", "c"}) {
    const Style *style = registry.style(name);
    ASSERT_NE(nullptr, style) << name;
    for (std::size_t depth = 0; style != nullptr; ++depth) {
      ASSERT_LT(depth, 3u) << name;
      style = style->parent();
    }
  }
}

/// An unknown `w:basedOn` target leaves the style parentless.
TEST(ooxml_text_style, unknown_based_on_target) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(
      R"(<w:styles><w:style w:styleId="a"><w:basedOn w:val="gone"/></w:style></w:styles>)",
      document);

  const Style *style = registry.style("a");
  ASSERT_NE(nullptr, style);
  EXPECT_EQ(nullptr, style->parent());
  EXPECT_EQ(nullptr, registry.style("gone"));
}

TEST(ooxml_text_style, paragraph_spacing) {
  pugi::xml_document document;
  const pugi::xml_node paragraph = node_of(
      R"(<w:p><w:pPr><w:spacing w:before="240" w:after="120" w:line="360"/></w:pPr></w:p>)",
      document);

  const ParagraphStyle style =
      StyleRegistry().partial_paragraph_style(paragraph).paragraph_style;

  ASSERT_TRUE(style.margin.top.has_value());
  EXPECT_EQ(Measure(240 / 1440.0, DynamicUnit("in")), *style.margin.top);
  ASSERT_TRUE(style.margin.bottom.has_value());
  EXPECT_EQ(Measure(120 / 1440.0, DynamicUnit("in")), *style.margin.bottom);
  // `w:line` without a rule is `auto`: 240ths of a line
  ASSERT_TRUE(style.line_height.has_value());
  EXPECT_EQ(Measure(150, DynamicUnit("%")), *style.line_height);
}

TEST(ooxml_text_style, paragraph_spacing_exact_line) {
  pugi::xml_document document;
  const pugi::xml_node paragraph = node_of(
      R"(<w:p><w:pPr><w:spacing w:line="480" w:lineRule="exact"/></w:pPr></w:p>)",
      document);

  const ParagraphStyle style =
      StyleRegistry().partial_paragraph_style(paragraph).paragraph_style;

  ASSERT_TRUE(style.line_height.has_value());
  EXPECT_EQ(Measure(480 / 1440.0, DynamicUnit("in")), *style.line_height);
}

/// An autospacing flag makes word ignore the value written next to it.
TEST(ooxml_text_style, paragraph_spacing_autospacing) {
  pugi::xml_document document;
  const pugi::xml_node paragraph = node_of(
      R"(<w:p><w:pPr><w:spacing w:before="100" w:after="100")"
      R"( w:beforeAutospacing="1" w:afterAutospacing="on"/></w:pPr></w:p>)",
      document);

  const ParagraphStyle style =
      StyleRegistry().partial_paragraph_style(paragraph).paragraph_style;

  EXPECT_FALSE(style.margin.top.has_value());
  EXPECT_FALSE(style.margin.bottom.has_value());
}

TEST(ooxml_text_style, table_row_height) {
  pugi::xml_document document;
  const pugi::xml_node row = node_of(
      R"(<w:tr><w:trPr><w:trHeight w:val="720"/></w:trPr></w:tr>)", document);

  const TableRowStyle style =
      StyleRegistry().partial_table_row_style(row).table_row_style;

  ASSERT_TRUE(style.height.has_value());
  EXPECT_EQ(Measure(720 / 1440.0, DynamicUnit("in")), *style.height);
}

/// `auto` lets the row grow with its content, which html does on its own.
TEST(ooxml_text_style, table_row_height_auto) {
  pugi::xml_document document;
  const pugi::xml_node row = node_of(
      R"(<w:tr><w:trPr><w:trHeight w:val="720" w:hRule="auto"/></w:trPr></w:tr>)",
      document);

  const TableRowStyle style =
      StyleRegistry().partial_table_row_style(row).table_row_style;

  EXPECT_FALSE(style.height.has_value());
}

/// `w:contextualSpacing` drops the spacing towards a neighbour of the same
/// style, and only towards one.
TEST(ooxml_text_style, paragraph_contextual_spacing) {
  pugi::xml_document document;
  const pugi::xml_node body = node_of(
      R"(<w:body>)"
      R"(<w:p><w:pPr><w:pStyle w:val="list"/><w:spacing w:before="240" w:after="240"/><w:contextualSpacing/></w:pPr></w:p>)"
      R"(<w:p><w:pPr><w:pStyle w:val="list"/><w:spacing w:before="240" w:after="240"/><w:contextualSpacing/></w:pPr></w:p>)"
      R"(<w:p><w:pPr><w:pStyle w:val="other"/></w:pPr></w:p>)"
      R"(</w:body>)",
      document);

  const ParagraphStyle style =
      StyleRegistry()
          .partial_paragraph_style(body.last_child().previous_sibling())
          .paragraph_style;

  // the paragraph above is a `list` too, the one below is not
  ASSERT_TRUE(style.margin.top.has_value());
  EXPECT_EQ(Measure(0, DynamicUnit("in")), *style.margin.top);
  ASSERT_TRUE(style.margin.bottom.has_value());
  EXPECT_EQ(Measure(240 / 1440.0, DynamicUnit("in")), *style.margin.bottom);
}

/// A table style also carries the properties of the paragraphs in the table.
TEST(ooxml_text_style, table_style_reference) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(
      R"(<w:styles><w:style w:styleId="grid">)"
      R"(<w:pPr><w:spacing w:after="0"/><w:jc w:val="center"/></w:pPr>)"
      R"(</w:style></w:styles>)",
      document);

  pugi::xml_document table_document;
  const pugi::xml_node table =
      node_of(R"(<w:tbl><w:tblPr><w:tblStyle w:val="grid"/></w:tblPr></w:tbl>)",
              table_document);

  const ParagraphStyle style =
      registry.partial_table_style(table).paragraph_style;

  ASSERT_TRUE(style.margin.bottom.has_value());
  EXPECT_EQ(Measure(0, DynamicUnit("in")), *style.margin.bottom);
  ASSERT_TRUE(style.text_align.has_value());
  EXPECT_EQ(TextAlign::center, *style.text_align);
}

/// A `w:sdt` renders as nothing but its children, and a marker element is not
/// content either, so neither breaks the neighbourhood.
TEST(ooxml_text_style, paragraph_contextual_spacing_through_wrappers) {
  pugi::xml_document document;
  const pugi::xml_node body = node_of(
      R"(<w:body>)"
      R"(<w:sdt><w:sdtContent>)"
      R"(<w:p><w:pPr><w:pStyle w:val="list"/><w:spacing w:after="240"/><w:contextualSpacing/></w:pPr></w:p>)"
      R"(</w:sdtContent></w:sdt>)"
      R"(<w:bookmarkEnd w:id="1"/>)"
      R"(<w:p><w:pPr><w:pStyle w:val="list"/><w:spacing w:before="240" w:after="240"/><w:contextualSpacing/></w:pPr></w:p>)"
      R"(<w:tbl/>)"
      R"(</w:body>)",
      document);

  const StyleRegistry registry;
  const ParagraphStyle inside_wrapper =
      registry
          .partial_paragraph_style(
              body.child("w:sdt").child("w:sdtContent").child("w:p"))
          .paragraph_style;
  const ParagraphStyle after_wrapper =
      registry.partial_paragraph_style(body.child("w:p")).paragraph_style;

  // the paragraph below reaches out of the wrapper and past the marker
  ASSERT_TRUE(inside_wrapper.margin.bottom.has_value());
  EXPECT_EQ(Measure(0, DynamicUnit("in")), *inside_wrapper.margin.bottom);
  // and is seen from the other side too, while the table below is not a
  // paragraph of the same style
  ASSERT_TRUE(after_wrapper.margin.top.has_value());
  EXPECT_EQ(Measure(0, DynamicUnit("in")), *after_wrapper.margin.top);
  ASSERT_TRUE(after_wrapper.margin.bottom.has_value());
  EXPECT_EQ(Measure(240 / 1440.0, DynamicUnit("in")),
            *after_wrapper.margin.bottom);
}
