#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <odr/style.hpp>

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

/// [ECMA-376] 17.3.1.23. `w:val="0"` clears an inherited break, so off has to
/// be told from unsaid. `sample1.docx`'s `TOCHeading` is such a style.
TEST(ooxml_text_style,
     page_break_before_is_inherited_until_a_style_turns_it_off) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(
      R"(<w:styles>)"
      R"(<w:style w:styleId="head"><w:pPr><w:pageBreakBefore/></w:pPr></w:style>)"
      R"(<w:style w:styleId="derived"><w:basedOn w:val="head"/></w:style>)"
      R"(<w:style w:styleId="toc"><w:basedOn w:val="head"/>)"
      R"(<w:pPr><w:pageBreakBefore w:val="0"/></w:pPr></w:style>)"
      R"(<w:style w:styleId="plain"/>)"
      R"(</w:styles>)",
      document);

  const auto break_before = [&](const char *name) {
    const Style *style = registry.style(name);
    EXPECT_NE(nullptr, style);
    return style->resolved().paragraph_style.break_before;
  };

  EXPECT_EQ(BreakType::page, break_before("head"));
  EXPECT_EQ(BreakType::page, break_before("derived"));
  EXPECT_EQ(BreakType::none, break_before("toc"));
  EXPECT_FALSE(break_before("plain").has_value());
}

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

/// [ECMA-376] 17.4.39; `w:sz` is in eighths of a point.
TEST(ooxml_text_style, table_borders) {
  pugi::xml_document document;
  const pugi::xml_node table =
      node_of(R"(<w:tbl><w:tblPr><w:tblBorders>)"
              R"(<w:top w:val="single" w:sz="8" w:color="000000"/>)"
              R"(<w:left w:val="single" w:sz="8" w:color="000000"/>)"
              R"(<w:bottom w:val="single" w:sz="8" w:color="000000"/>)"
              R"(<w:right w:val="single" w:sz="8" w:color="000000"/>)"
              R"(<w:insideH w:val="single" w:sz="4" w:color="808080"/>)"
              R"(<w:insideV w:val="single" w:sz="4" w:color="808080"/>)"
              R"(</w:tblBorders></w:tblPr></w:tbl>)",
              document);

  const TableStyle style =
      StyleRegistry().partial_table_style(table).table_style;

  EXPECT_EQ("1pt solid #000000", style.border.top);
  EXPECT_EQ("1pt solid #000000", style.border.left);
  EXPECT_EQ("1pt solid #000000", style.border.bottom);
  EXPECT_EQ("1pt solid #000000", style.border.right);
  EXPECT_EQ("0.5pt solid #808080", style.border_inside_horizontal);
  EXPECT_EQ("0.5pt solid #808080", style.border_inside_vertical);
}

/// A `w:tblStyle` carries the borders down as it carries the rest.
TEST(ooxml_text_style, table_borders_through_the_style_reference) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(
      R"(<w:styles><w:style w:styleId="grid"><w:tblPr><w:tblBorders>)"
      R"(<w:top w:val="single" w:sz="8" w:color="000000"/>)"
      R"(<w:insideH w:val="single" w:sz="8" w:color="000000"/>)"
      R"(</w:tblBorders></w:tblPr></w:style></w:styles>)",
      document);

  pugi::xml_document table_document;
  const pugi::xml_node table =
      node_of(R"(<w:tbl><w:tblPr><w:tblStyle w:val="grid"/><w:tblBorders>)"
              R"(<w:top w:val="single" w:sz="24" w:color="FF0000"/>)"
              R"(</w:tblBorders></w:tblPr></w:tbl>)",
              table_document);

  const TableStyle style = registry.partial_table_style(table).table_style;

  // the table's own beats the style's, side by side
  EXPECT_EQ("3pt solid #ff0000", style.border.top);
  EXPECT_EQ("1pt solid #000000", style.border_inside_horizontal);
}

/// `nil` and `none` draw nothing, `auto` leaves the colour to the text.
TEST(ooxml_text_style, a_cell_border_of_nil_is_not_silence) {
  pugi::xml_document document;
  const pugi::xml_node cell =
      node_of(R"(<w:tc><w:tcPr><w:tcBorders>)"
              R"(<w:top w:val="nil"/>)"
              R"(<w:bottom w:val="none" w:sz="4"/>)"
              R"(<w:right w:val="single" w:sz="8" w:color="auto"/>)"
              R"(</w:tcBorders></w:tcPr></w:tc>)",
              document);

  const TableCellStyle style =
      StyleRegistry().partial_table_cell_style(cell).table_cell_style;

  EXPECT_EQ("0 none", style.border.top);
  EXPECT_EQ("0 none", style.border.bottom);
  EXPECT_EQ("1pt solid", style.border.right);
  EXPECT_FALSE(style.border.left.has_value());
}

namespace {

TableStyle grid_table_style() {
  TableStyle result;
  result.border = DirectionalStyle<std::string>(std::string("frame"));
  result.border_inside_horizontal = "rule-h";
  result.border_inside_vertical = "rule-v";
  return result;
}

pugi::xml_node cell_at(const pugi::xml_node table, const std::size_t row,
                       const std::size_t column) {
  pugi::xml_node row_node = table.child("w:tr");
  for (std::size_t i = 0; i < row; ++i) {
    row_node = row_node.next_sibling("w:tr");
  }
  pugi::xml_node cell_node = row_node.child("w:tc");
  for (std::size_t i = 0; i < column; ++i) {
    cell_node = cell_node.next_sibling("w:tc");
  }
  return cell_node;
}

/// `table_cell_border` for a plain grid, where the cell above is one row up.
DirectionalStyle<std::string> border_at(const pugi::xml_node table,
                                        const std::size_t row,
                                        const std::size_t column,
                                        const TableStyle &table_style,
                                        const std::uint32_t rows = 1) {
  const pugi::xml_node above =
      row == 0 ? pugi::xml_node() : cell_at(table, row - 1, column);
  return table_cell_border(cell_at(table, row, column), above, table_style,
                           rows);
}

} // namespace

/// A grid line is drawn once, by the cell that leads it.
TEST(ooxml_text_style, a_cell_draws_only_the_table_borders_it_leads) {
  pugi::xml_document document;
  const pugi::xml_node table = node_of(R"(<w:tbl>)"
                                       R"(<w:tr><w:tc/><w:tc/></w:tr>)"
                                       R"(<w:tr><w:tc/><w:tc/></w:tr>)"
                                       R"(</w:tbl>)",
                                       document);
  const TableStyle table_style = grid_table_style();

  const DirectionalStyle<std::string> top_left =
      border_at(table, 0, 0, table_style);
  EXPECT_EQ("frame", top_left.top);
  EXPECT_EQ("frame", top_left.left);
  EXPECT_FALSE(top_left.bottom.has_value());
  EXPECT_FALSE(top_left.right.has_value());

  const DirectionalStyle<std::string> bottom_right =
      border_at(table, 1, 1, table_style);
  EXPECT_EQ("rule-h", bottom_right.top);
  EXPECT_EQ("rule-v", bottom_right.left);
  EXPECT_EQ("frame", bottom_right.bottom);
  EXPECT_EQ("frame", bottom_right.right);
}

/// A rule the trailing cell states is drawn by the leading one, once.
TEST(ooxml_text_style, a_rule_the_trailing_cell_states_is_drawn_once) {
  pugi::xml_document document;
  const pugi::xml_node table = node_of(
      R"(<w:tbl>)"
      R"(<w:tr>)"
      R"(<w:tc><w:tcPr><w:tcBorders><w:right w:val="single" w:sz="24" w:color="FF0000"/><w:bottom w:val="single" w:sz="24" w:color="00FF00"/></w:tcBorders></w:tcPr></w:tc>)"
      R"(<w:tc/>)"
      R"(</w:tr>)"
      R"(<w:tr><w:tc/><w:tc/></w:tr>)"
      R"(</w:tbl>)",
      document);
  const TableStyle table_style = grid_table_style();

  const DirectionalStyle<std::string> stating =
      border_at(table, 0, 0, table_style);
  EXPECT_FALSE(stating.right.has_value());
  EXPECT_FALSE(stating.bottom.has_value());
  EXPECT_EQ("3pt solid #ff0000", border_at(table, 0, 1, table_style).left);
  EXPECT_EQ("3pt solid #00ff00", border_at(table, 1, 0, table_style).top);
}

/// A cell's own border beats what the neighbour and the table say.
TEST(ooxml_text_style, a_cell_border_beats_the_neighbours_and_the_tables) {
  pugi::xml_document document;
  const pugi::xml_node table = node_of(
      R"(<w:tbl>)"
      R"(<w:tr><w:tc/><w:tc><w:tcPr><w:tcBorders><w:left w:val="single" w:sz="24" w:color="FF0000"/></w:tcBorders></w:tcPr></w:tc></w:tr>)"
      R"(<w:tr><w:tc><w:tcPr><w:tcBorders><w:top w:val="nil"/></w:tcBorders></w:tcPr></w:tc><w:tc/></w:tr>)"
      R"(</w:tbl>)",
      document);
  const TableStyle table_style = grid_table_style();

  EXPECT_EQ("3pt solid #ff0000", border_at(table, 0, 1, table_style).left);
  EXPECT_EQ("0 none", border_at(table, 1, 0, table_style).top);
}

/// A covered cell never renders, so the cell merged over it closes the frame.
TEST(ooxml_text_style, a_merged_cell_closes_the_frame_it_reaches) {
  pugi::xml_document document;
  const pugi::xml_node table = node_of(R"(<w:tbl>)"
                                       R"(<w:tr><w:tc/></w:tr>)"
                                       R"(<w:tr><w:tc/></w:tr>)"
                                       R"(<w:tr><w:tc/></w:tr>)"
                                       R"(</w:tbl>)",
                                       document);
  const TableStyle table_style = grid_table_style();

  EXPECT_EQ("frame", border_at(table, 1, 0, table_style, 2).bottom);
  EXPECT_FALSE(border_at(table, 1, 0, table_style, 1).bottom.has_value());
  // a merge running past the last row still closes it
  EXPECT_EQ("frame", border_at(table, 1, 0, table_style, 9).bottom);
}

/// The cell above sits at this one's grid column, which `w:gridSpan` moves.
TEST(ooxml_text_style, a_spanned_cell_is_led_by_the_column_it_starts_at) {
  pugi::xml_document document;
  const pugi::xml_node table = node_of(
      R"(<w:tbl>)"
      R"(<w:tr><w:tc><w:tcPr><w:gridSpan w:val="2"/><w:tcBorders><w:bottom w:val="single" w:sz="24" w:color="FF0000"/></w:tcBorders></w:tcPr></w:tc><w:tc/></w:tr>)"
      R"(<w:tr><w:tc/><w:tc/><w:tc/></w:tr>)"
      R"(</w:tbl>)",
      document);
  const TableStyle table_style = grid_table_style();

  // the spanning cell covers grid columns 0 and 1
  EXPECT_EQ("3pt solid #ff0000",
            table_cell_border(cell_at(table, 1, 0), cell_at(table, 0, 0),
                              table_style, 1)
                .top);
  EXPECT_EQ("rule-h", table_cell_border(cell_at(table, 1, 2),
                                        cell_at(table, 0, 1), table_style, 1)
                          .top);
  EXPECT_EQ("frame", border_at(table, 0, 1, table_style).right);
}

/// `TextWrap::before` leaves the text on the frame's left.
TEST(ooxml_text_style, frame_wrap_takes_the_side_from_wrap_text) {
  const auto wrap_of = [](const char *xml) {
    pugi::xml_document document;
    return read_frame_style(node_of(xml, document)).text_wrap;
  };

  EXPECT_EQ(
      TextWrap::before,
      wrap_of(R"(<wp:anchor><wp:wrapSquare wrapText="left"/></wp:anchor>)"));
  EXPECT_EQ(
      TextWrap::after,
      wrap_of(R"(<wp:anchor><wp:wrapTight wrapText="right"/></wp:anchor>)"));
  EXPECT_EQ(
      TextWrap::before,
      wrap_of(R"(<wp:anchor><wp:wrapThrough wrapText="left"/></wp:anchor>)"));
  EXPECT_EQ(TextWrap::none,
            wrap_of(R"(<wp:anchor><wp:wrapTopAndBottom/></wp:anchor>)"));
  EXPECT_EQ(TextWrap::run_through,
            wrap_of(R"(<wp:anchor><wp:wrapNone/></wp:anchor>)"));
  // a `wp:inline` states no wrap at all
  EXPECT_FALSE(wrap_of(R"(<wp:inline/>)").has_value());
}

/// Where word wraps both sides, the frame's own side picks the float's.
TEST(ooxml_text_style, frame_wrap_on_both_sides_follows_the_frame) {
  const auto style_of = [](const char *align, const char *wrap_text) {
    pugi::xml_document document;
    const std::string xml =
        R"(<wp:anchor><wp:positionH relativeFrom="margin"><wp:align>)" +
        std::string(align) + R"(</wp:align></wp:positionH>)" +
        R"(<wp:wrapSquare wrapText=")" + std::string(wrap_text) +
        R"("/></wp:anchor>)";
    return read_frame_style(node_of(xml.c_str(), document));
  };

  EXPECT_EQ(HorizontalAlign::left,
            style_of("left", "bothSides").horizontal_position);
  EXPECT_EQ(TextWrap::after, style_of("left", "bothSides").text_wrap);
  EXPECT_EQ(TextWrap::before, style_of("right", "bothSides").text_wrap);
  EXPECT_EQ(TextWrap::none, style_of("center", "largest").text_wrap);
  EXPECT_EQ(HorizontalAlign::center,
            style_of("center", "largest").horizontal_position);
}

/// A page-relative offset has no meaning for a frame that stays in the flow.
TEST(ooxml_text_style, frame_offset_is_read_where_it_flows_with_the_text) {
  pugi::xml_document document;
  const pugi::xml_node anchor = node_of(
      R"(<wp:anchor>)"
      R"(<wp:positionH relativeFrom="column"><wp:posOffset>2286000</wp:posOffset></wp:positionH>)"
      R"(<wp:positionV relativeFrom="page"><wp:posOffset>457200</wp:posOffset></wp:positionV>)"
      R"(<wp:simplePos><wp:posOffset>-457200</wp:posOffset></wp:simplePos>)"
      R"(</wp:anchor>)",
      document);

  EXPECT_EQ(Measure(2.5, DynamicUnit("in")),
            read_frame_offset(anchor.child("wp:positionH")));
  EXPECT_FALSE(read_frame_offset(anchor.child("wp:positionV")).has_value());
  // no `relativeFrom` is not the page
  EXPECT_EQ(Measure(-0.5, DynamicUnit("in")),
            read_frame_offset(anchor.child("wp:simplePos")));
  EXPECT_FALSE(read_frame_offset(anchor.child("wp:noSuchChild")).has_value());
}
