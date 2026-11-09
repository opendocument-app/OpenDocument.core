#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;

namespace {

std::string flat_text(const std::string &body, const std::string &styles) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.text">)" +
         styles + "<office:body><office:text>" + body +
         "</office:text></office:body></office:document>";
}

/// Level 2 of the outline as Word writes it, "1.1".
constexpr const char *default_level_two =
    R"(style:num-format="1" text:display-levels="2")";

/// An outline numbering levels 1 to 3 as Word writes it: "1", "1.1",
/// "1.1.1", with @p level_two stating level 2.
std::string outline_style(const std::string &level_two = default_level_two) {
  return R"(<office:styles><text:outline-style style:name="Outline">)"
         R"(<text:outline-level-style text:level="1" style:num-format="1"/>)"
         R"(<text:outline-level-style text:level="2" )" +
         level_two +
         R"(/><text:outline-level-style text:level="3" style:num-format="1")"
         R"( text:display-levels="3"/>)"
         R"(</text:outline-style></office:styles>)";
}

std::string numbered_outline(const std::string &body,
                             const std::string &level_two = default_level_two) {
  return flat_text(body, outline_style(level_two));
}

/// A list style numbering levels 1 and 2 "1." and "1.1.".
constexpr const char *numbered_list_style =
    R"(<office:automatic-styles><text:list-style style:name="L1">)"
    R"(<text:list-level-style-number text:level="1" style:num-format="1")"
    R"( style:num-suffix="."/>)"
    R"(<text:list-level-style-number text:level="2" style:num-format="1")"
    R"( style:num-suffix="." text:display-levels="2"/>)"
    R"(</text:list-style></office:automatic-styles>)";

Document document_of(const std::string &source) {
  return open(File::from_memory(source)).as_document_file().document();
}

/// The marker of every top-level paragraph.
std::vector<std::string> heading_markers(const std::string &source) {
  const Document document = document_of(source);
  std::vector<std::string> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_paragraph().marker());
  }
  return result;
}

void collect_item_markers(const Element element,
                          std::vector<std::string> &result) {
  for (const Element child : element.children()) {
    if (child.type() == ElementType::list_item) {
      result.push_back(child.as_list_item().marker());
    }
    collect_item_markers(child, result);
  }
}

/// The marker of every list item, in document order.
std::vector<std::string> item_markers(const std::string &source) {
  const Document document = document_of(source);
  std::vector<std::string> result;
  collect_item_markers(document.root_element(), result);
  return result;
}

std::string render(const std::string &source) {
  const HtmlConfig config(
      (std::filesystem::current_path() / "odf_numbering").string());
  std::ostringstream out;
  html::translate(open(File::from_memory(source)), config)
      .list_views()
      .at(0)
      .write_html(out);
  return out.str();
}

std::size_t count(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + needle.size())) {
    ++result;
  }
  return result;
}

std::string heading(const std::uint64_t level, const std::string &text,
                    const std::string &attributes = "") {
  return R"(<text:h text:outline-level=")" + std::to_string(level) + "\" " +
         attributes + ">" + text + "</text:h>";
}

/// A list of style L1 holding @p items, which continues the numbering where
/// @p continues.
std::string list(const std::string &items, const bool continues = false) {
  return std::string(R"(<text:list text:style-name="L1")") +
         (continues ? R"( text:continue-numbering="true")" : "") + ">" + items +
         "</text:list>";
}

std::string item(const std::string &content) {
  return "<text:list-item>" + content + "</text:list-item>";
}

std::string paragraph(const std::string &text) {
  return "<text:p>" + text + "</text:p>";
}

} // namespace

TEST(OdfOutline, headings_take_the_numbers_of_the_outline_style) {
  EXPECT_EQ(heading_markers(numbered_outline(
                heading(1, "Access") + heading(2, "Introduction") +
                paragraph("body") + heading(2, "License") +
                heading(1, "Requests") + heading(2, "Format") +
                heading(1, "Annex", R"(text:is-list-header="true")"))),
            (std::vector<std::string>{"1", "1.1", "", "1.2", "2", "2.1", ""}));
}

TEST(OdfOutline, a_skipped_level_opens_at_its_start_and_counts_on) {
  // LibreOffice: 1, 1.1.1, then the skipped level's second, 1.2
  EXPECT_EQ(heading_markers(numbered_outline(heading(1, "a") + heading(3, "b") +
                                             heading(2, "c"))),
            (std::vector<std::string>{"1", "1.1.1", "1.2"}));
}

TEST(OdfOutline, a_level_without_a_format_is_left_out_with_its_period) {
  // LibreOffice: level 2 shows its prefix and suffix only, level 3 skips it
  EXPECT_EQ(heading_markers(numbered_outline(
                heading(1, "a") + heading(2, "b") + heading(3, "c"),
                R"(style:num-format="" style:num-prefix="(")"
                R"x( style:num-suffix=")" text:display-levels="2")x")),
            (std::vector<std::string>{"1", "()", "1.1"}));
}

TEST(OdfOutline, a_heading_can_restart_the_outline) {
  EXPECT_EQ(
      heading_markers(numbered_outline(
          heading(1, "one") +
          heading(1, "five",
                  R"(text:restart-numbering="true" text:start-value="5")") +
          heading(1, "restarted", R"(text:restart-numbering="true")"))),
      (std::vector<std::string>{"1", "5", "1"}));
}

TEST(OdfOutline, a_level_deeper_than_ten_is_the_tenth) {
  // the level sizes the counters, so four billion must not allocate them;
  // the tenth level takes the deepest style stated, which shows three levels
  EXPECT_EQ(heading_markers(numbered_outline(heading(1, "a") +
                                             heading(4000000000, "deep"))),
            (std::vector<std::string>{"1", "1.1.1"}));
}

TEST(OdfOutline, a_heading_of_level_zero_is_not_numbered) {
  EXPECT_EQ(heading_markers(numbered_outline(heading(0, "a"))),
            (std::vector<std::string>{""}));
}

TEST(OdfOutline, headings_without_an_outline_style_are_not_numbered) {
  EXPECT_EQ(heading_markers(flat_text(heading(1, "one"), "")),
            (std::vector<std::string>{""}));
}

TEST(OdfOutline, a_heading_in_a_list_is_numbered_as_an_item) {
  const std::string source = flat_text(list(item(heading(1, "in a list"))),
                                       outline_style() + numbered_list_style);
  EXPECT_EQ(item_markers(source), (std::vector<std::string>{"1."}));
  const Document document = document_of(source);
  const Element heading_in_list =
      document.root_element().first_child().first_child().first_child();
  EXPECT_EQ(heading_in_list.as_paragraph().marker(), "");
}

TEST(OdfOutline, a_list_may_number_by_the_outline) {
  EXPECT_EQ(item_markers(
                numbered_outline(R"(<text:list text:style-name="Outline">)" +
                                 item(paragraph("a") + R"(<text:list>)" +
                                      item(paragraph("b")) + "</text:list>") +
                                 "</text:list>")),
            (std::vector<std::string>{"1", "1.1"}));
}

TEST(OdfOutline, a_numbered_heading_is_drawn_with_its_number_in_front) {
  const std::string html = render(numbered_outline(heading(1, "Access")));
  // not as a list item's label, which hangs into an indent a heading lacks
  EXPECT_EQ(count(html, R"(class="odr-heading-marker")"), 1U);
  EXPECT_EQ(count(html, R"(class="odr-list-marker")"), 0U);
  EXPECT_EQ(count(html, "1&#9;"), 1U);
}

/// Word writes an item of level 2 as an item of level 1 that holds only the
/// nested list, each in a list of its own that continues the numbering.
TEST(OdfList, an_item_without_a_paragraph_is_not_counted) {
  const std::string nested = list(item(list(item(paragraph("sub")))), true);
  EXPECT_EQ(item_markers(flat_text(list(item(paragraph("Details"))) + nested +
                                       nested +
                                       list(item(paragraph("Statement")), true),
                                   numbered_list_style)),
            (std::vector<std::string>{"1.", "", "1.1.", "", "1.2.", "2."}));
}

TEST(OdfList, a_list_opening_on_a_deeper_level_counts_the_skipped_one) {
  // LibreOffice: 1.1. E, 1.2. F, 2. G
  EXPECT_EQ(item_markers(flat_text(
                list(item(list(item(paragraph("E")) + item(paragraph("F")))) +
                     item(paragraph("G"))),
                numbered_list_style)),
            (std::vector<std::string>{"", "1.1.", "1.2.", "2."}));
}

TEST(OdfList, a_table_page_break_or_cached_label_before_the_paragraph_count) {
  EXPECT_EQ(item_markers(flat_text(
                list(item(paragraph("a")) +
                     item(R"(<table:table><table:table-row><table:table-cell>)"
                          R"(<text:p>cell</text:p></table:table-cell>)"
                          R"(</table:table-row></table:table>)" +
                          paragraph("b")) +
                     item("<text:soft-page-break/>" + paragraph("c")) +
                     item("<text:number>4.</text:number>" + paragraph("d"))),
                numbered_list_style)),
            (std::vector<std::string>{"1.", "2.", "3.", "4."}));
}
