#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace odr;

namespace {

std::string flat_document(const std::string &mimetype, const std::string &body,
                          const std::string &styles = "") {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")" +
         mimetype + R"(">)" + styles + "<office:body>" + body +
         "</office:body></office:document>";
}

/// The text of the runs under @p element, fields included.
std::string text_under(const Element element) {
  std::string result;
  for (const Element child : element.children()) {
    result += child.type() == ElementType::text ? child.as_text().content()
                                                : text_under(child);
  }
  return result;
}

/// The text of the first paragraph of a text document whose body is
/// @p paragraph.
std::string text_of(const std::string &paragraph) {
  const Document document =
      open(File::from_memory(
               flat_document("application/vnd.oasis.opendocument.text",
                             "<office:text>" + paragraph + "</office:text>")))
          .as_document_file()
          .document();
  return text_under(document.root_element().first_child());
}

} // namespace

TEST(OdfField, a_field_shows_the_text_its_producer_computed) {
  EXPECT_EQ(text_of(R"(<text:p>Page <text:page-number)"
                    R"( text:select-page="current">1</text:page-number>)"
                    R"( of <text:page-count>5</text:page-count></text:p>)"),
            "Page 1 of 5");
  EXPECT_EQ(text_of(R"(<text:p>Figure <text:sequence text:ref-name="r")"
                    R"( text:name="Figure" style:num-format="1">3)"
                    R"(</text:sequence>: Map</text:p>)"),
            "Figure 3: Map");
  EXPECT_EQ(text_of(R"(<text:p>see <text:bookmark-ref)"
                    R"( text:reference-format="text" text:ref-name="b">)"
                    R"(Annex B</text:bookmark-ref></text:p>)"),
            "see Annex B");
  EXPECT_EQ(text_of(R"(<text:p><text:title>Budget 2024</text:title></text:p>)"),
            "Budget 2024");
  EXPECT_EQ(text_of(R"(<text:p><text:meta-field xml:id="m">tagged)"
                    R"(</text:meta-field></text:p>)"),
            "tagged");
}

TEST(OdfField, a_field_displayed_as_none_shows_nothing) {
  EXPECT_EQ(text_of(R"(<text:p>a<text:variable-set text:name="v")"
                    R"( text:display="none" office:value-type="float")"
                    R"( office:value="5">5</text:variable-set>b</text:p>)"),
            "ab");
  EXPECT_EQ(
      text_of(R"(<text:p><text:user-field-get text:name="u")"
              R"( text:display="none">42</text:user-field-get></text:p>)"),
      "");
  EXPECT_EQ(text_of(R"(<text:p><text:variable-get text:name="v")"
                    R"( text:display="value">5</text:variable-get></text:p>)"),
            "5");
}

TEST(OdfField, a_field_without_text_leaves_its_paragraph_empty) {
  HtmlConfig config;
  config.editable = true;
  std::ostringstream html;
  html::translate(open(File::from_memory(flat_document(
                      "application/vnd.oasis.opendocument.text",
                      R"(<office:text><text:p><text:hidden-text)"
                      R"( text:condition="ooow:1==1" text:string-value="x"/>)"
                      R"(</text:p></office:text>)"))),
                  config)
      .list_views()
      .at(0)
      .write_html(html);
  // so the paragraph keeps the height of an empty line
  EXPECT_NE(html.str().find("<br></x-p>"), std::string::npos);
}

TEST(OdfField, hidden_text_shows_unless_saved_as_hidden) {
  EXPECT_EQ(text_of(R"(<text:p>shown<text:hidden-text text:condition="ooow:1")"
                    R"( text:string-value="no" text:is-hidden="true">no)"
                    R"(</text:hidden-text></text:p>)"),
            "shown");
  EXPECT_EQ(text_of(R"(<text:p><text:hidden-text text:condition="ooow:0")"
                    R"( text:string-value="yes" text:is-hidden="false">yes)"
                    R"(</text:hidden-text></text:p>)"),
            "yes");
  // `text:is-hidden` defaults to false
  EXPECT_EQ(
      text_of(R"(<text:p><text:hidden-text text:condition="ooow:0")"
              R"( text:string-value="yes">yes</text:hidden-text></text:p>)"),
      "yes");
}

TEST(OdfRuby, shows_its_base_text) {
  EXPECT_EQ(text_of(R"(<text:p><text:ruby><text:ruby-base>漢字)"
                    R"(</text:ruby-base><text:ruby-text>かんじ)"
                    R"(</text:ruby-text></text:ruby></text:p>)"),
            "漢字");
}

TEST(OdfRuby, is_not_editable_and_moves_whole) {
  const Document document =
      open(File::from_memory(flat_document(
               "application/vnd.oasis.opendocument.text",
               R"(<office:text><text:p>A<text:ruby><text:ruby-base>漢字)"
               R"(</text:ruby-base><text:ruby-text>かんじ</text:ruby-text>)"
               R"(</text:ruby>B</text:p></office:text>)")))
          .as_document_file()
          .document();
  const Element paragraph = document.root_element().first_child();
  const Element ruby = paragraph.first_child().next_sibling();
  ASSERT_EQ(ruby.type(), ElementType::group);
  EXPECT_FALSE(ruby.is_editable());
  EXPECT_FALSE(ruby.first_child().first_child().is_editable());
  EXPECT_TRUE(ruby.next_sibling().is_editable());

  const Paragraph second =
      document.split_paragraph(paragraph.as_paragraph(), ruby);
  EXPECT_EQ(text_under(second), "B");
  const std::string saved(document.save_to_memory().memory_data().value());
  EXPECT_NE(saved.find(R"(<text:p>A<text:ruby><text:ruby-base>漢字)"
                       R"(</text:ruby-base><text:ruby-text>かんじ)"
                       R"(</text:ruby-text></text:ruby></text:p><text:p>B)"),
            std::string::npos);
}

TEST(OdfField, a_page_number_impress_left_uncomputed_shows_nothing) {
  const Document document =
      open(File::from_memory(flat_document(
               "application/vnd.oasis.opendocument.presentation",
               R"(<office:presentation><draw:page draw:name="one">)"
               R"(<draw:frame svg:x="1cm" svg:y="1cm" svg:width="5cm")"
               R"( svg:height="1cm"><draw:text-box><text:p>Slide )"
               R"(<text:page-number>&lt;number&gt;</text:page-number>)"
               R"(</text:p></draw:text-box></draw:frame>)"
               R"(</draw:page></office:presentation>)")))
          .as_document_file()
          .document();
  EXPECT_EQ(text_under(document.root_element().first_child()), "Slide ");
  // in a text document the same text is what the producer saved
  EXPECT_EQ(text_of(R"(<text:p><text:page-number>&lt;7&gt;)"
                    R"(</text:page-number></text:p>)"),
            "<7>");
}

TEST(OdfField, a_field_is_not_editable) {
  const Document document =
      open(File::from_memory(
               flat_document("application/vnd.oasis.opendocument.text",
                             R"(<office:text><text:p>Page <text:page-count>5)"
                             R"(</text:page-count><text:hidden-text)"
                             R"( text:condition="ooow:0" text:string-value="x")"
                             R"( text:is-hidden="false">x</text:hidden-text>)"
                             R"(</text:p></office:text>)")))
          .as_document_file()
          .document();
  const Element paragraph = document.root_element().first_child();
  const Element run = paragraph.first_child();
  const Element field = run.next_sibling();
  ASSERT_EQ(field.type(), ElementType::group);
  EXPECT_TRUE(run.is_editable());
  EXPECT_FALSE(field.is_editable());
  EXPECT_FALSE(field.first_child().is_editable());
  // hidden text it shows is a field's text too
  const Element hidden_text = field.next_sibling();
  ASSERT_EQ(hidden_text.type(), ElementType::group);
  EXPECT_FALSE(hidden_text.first_child().is_editable());
}

TEST(OdfField, a_page_number_in_a_footer_shows_nothing) {
  // Word saved the last page's number into the footer of every page
  const Document document =
      open(
          File::from_memory(flat_document(
              "application/vnd.oasis.opendocument.text",
              R"(<office:text><text:p>one</text:p></office:text>)",
              R"(<office:master-styles><style:master-page)"
              R"( style:name="Standard"><style:footer><text:p>Page )"
              R"(<text:page-number>7</text:page-number> of )"
              R"(<text:page-count>7</text:page-count><text:page-variable-get>)"
              R"(3</text:page-variable-get><text:page-continuation)"
              R"( text:select-page="next">more</text:page-continuation></text:p>)"
              R"(</style:footer></style:master-page></office:master-styles>)")))
          .as_document_file()
          .document();
  const MasterPage master_page =
      document.root_element().as_text_root().first_master_page();
  ASSERT_TRUE(master_page);
  EXPECT_EQ(text_under(master_page.footer()), "Page  of 7");
}

TEST(OdfField, a_field_survives_a_merge_a_split_and_a_save) {
  const Document document =
      open(File::from_memory(flat_document(
               "application/vnd.oasis.opendocument.text",
               R"(<office:text><text:p>one</text:p><text:p>Page )"
               R"(<text:page-count>5</text:page-count><text:variable-set)"
               R"( text:name="v" text:display="none" office:value-type="float")"
               R"( office:value="5">5</text:variable-set></text:p>)"
               R"(</office:text>)")))
          .as_document_file()
          .document();
  const Paragraph first = document.root_element().first_child().as_paragraph();

  document.merge_paragraph_with_next(first);
  const Document merged =
      open(document.save_to_memory()).as_document_file().document();
  const Element merged_paragraph = merged.root_element().first_child();
  EXPECT_EQ(text_under(merged_paragraph), "onePage 5");
  EXPECT_FALSE(merged_paragraph.next_sibling());

  // "one" and "Page " read back as one run
  const Paragraph second = merged.split_paragraph(
      merged_paragraph.as_paragraph(), merged_paragraph.first_child());
  EXPECT_EQ(text_under(second), "5");
  const std::string saved(merged.save_to_memory().memory_data().value());
  EXPECT_NE(saved.find(R"(<text:p>onePage </text:p><text:p>)"
                       R"(<text:page-count>5</text:page-count>)"
                       R"(<text:variable-set text:name="v")"),
            std::string::npos);
}
