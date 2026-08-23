#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/markdown/markdown_file.hpp>
#include <odr/internal/text/text_file.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

/// An `Element` holds a bare pointer into its document, so every test binds the
/// document to a local before walking it.
Document document(const std::string &markdown) {
  const DecodedFile file(File::from_memory(markdown), FileType::markdown);
  return file.as_document_file().document();
}

Element root(const Document &document) { return document.root_element(); }

/// The direct children of an element, so a test can index into them.
std::vector<Element> children(const Element element) {
  std::vector<Element> result;
  for (const Element child : element.children()) {
    result.push_back(child);
  }
  return result;
}

/// Every text below @p element, concatenated — the reading text of a block,
/// whatever spans and links it is broken into.
std::string text_of(const Element element) {
  if (element.type() == ElementType::text) {
    return element.as_text().content();
  }
  std::string result;
  for (const Element child : element.children()) {
    result += text_of(child);
  }
  return result;
}

std::vector<ElementType> types_of(const std::vector<Element> &elements) {
  std::vector<ElementType> result;
  result.reserve(elements.size());
  for (const Element element : elements) {
    result.push_back(element.type());
  }
  return result;
}

} // namespace

TEST(MarkdownFile, a_markdown_file_is_a_text_document) {
  const DecodedFile file(File::from_memory("# hello"), FileType::markdown);
  const Document md = document("# hello");

  EXPECT_EQ(file.file_type(), FileType::markdown);
  EXPECT_EQ(file.file_category(), FileCategory::document);
  EXPECT_TRUE(file.is_document_file());
  EXPECT_FALSE(file.is_text_file());
  EXPECT_EQ(file.file_meta().mimetype, "text/markdown");
  EXPECT_EQ(file.as_document_file().document_type(), DocumentType::text);
  EXPECT_EQ(md.document_type(), DocumentType::text);
  EXPECT_EQ(root(md).type(), ElementType::root);
}

/// Markdown has no signature, and a content probe for it is a probe for prose,
/// so the caller routes on the file name and a `.md` still opens as text.
TEST(MarkdownFile, it_is_not_detected_by_content) {
  const File file = File::from_memory("# hello\n\nsome *markdown*\n");

  EXPECT_THAT(DecodedFile::list_file_types(file),
              testing::Not(testing::Contains(FileType::markdown)));
  EXPECT_EQ(DecodedFile(file).file_type(), FileType::text_file);
}

/// Nothing rejects: any UTF-8 byte sequence is some markdown document.
TEST(MarkdownFile, anything_decodable_reads_as_markdown) {
  EXPECT_NO_THROW(std::ignore = document("*"));
  EXPECT_NO_THROW(std::ignore = document("]]] not markup [[["));

  const Document empty = document("\n");
  EXPECT_FALSE(root(empty).first_child());
}

/// `Text::content()` is UTF-8 to every binding, so bytes we cannot decode have
/// no document at all — the text rendering path stays open to them.
TEST(MarkdownFile, an_undecodable_encoding_has_no_document) {
  const File file = File::from_memory("# hello");
  const internal::markdown::MarkdownFile markdown_file(
      std::make_shared<internal::text::TextFile>(file.impl(),
                                                 TextEncoding::shift_jis));

  EXPECT_FALSE(markdown_file.is_decodable());
  EXPECT_THROW(std::ignore = markdown_file.document(), UnsupportedTextEncoding);
}

TEST(MarkdownDocument, a_paragraph_is_a_paragraph) {
  const Document md = document("one\ntwo\n\nthree\n");
  const std::vector<Element> blocks = children(root(md));

  ASSERT_EQ(blocks.size(), 2);
  EXPECT_EQ(types_of(blocks),
            (std::vector{ElementType::paragraph, ElementType::paragraph}));
  // a soft break is a space, not a break
  EXPECT_EQ(text_of(blocks[0]), "one two");
  EXPECT_EQ(text_of(blocks[1]), "three");
}

TEST(MarkdownDocument, a_hard_break_is_a_line_break) {
  const Document md = document("one  \ntwo\n");
  const std::vector<Element> parts = children(children(root(md))[0]);

  EXPECT_EQ(types_of(parts),
            (std::vector{ElementType::text, ElementType::line_break,
                         ElementType::text}));
}

/// The model has no heading, so a heading is a paragraph whose text style
/// carries the level — the same compromise `odf_parser` makes for `text:h`.
TEST(MarkdownDocument, a_heading_is_a_paragraph_with_a_heading_style) {
  const Document md = document("# one\n\n### three\n");
  const std::vector<Element> blocks = children(root(md));

  ASSERT_EQ(blocks.size(), 2);
  EXPECT_EQ(types_of(blocks),
            (std::vector{ElementType::paragraph, ElementType::paragraph}));

  const TextStyle h1 = blocks[0].as_paragraph().text_style();
  const TextStyle h3 = blocks[1].as_paragraph().text_style();

  EXPECT_EQ(h1.font_weight, FontWeight::bold);
  EXPECT_EQ(h3.font_weight, FontWeight::bold);
  ASSERT_TRUE(h1.font_size.has_value());
  ASSERT_TRUE(h3.font_size.has_value());
  EXPECT_GT(h1.font_size->magnitude(), h3.font_size->magnitude());
  EXPECT_EQ(h1.font_size->unit().name(), "em");
}

TEST(MarkdownDocument, emphasis_becomes_a_span) {
  const Document md = document("a *b* **c** ~~d~~\n");
  const std::vector<Element> parts = children(children(root(md))[0]);

  ASSERT_EQ(parts.size(), 6);
  EXPECT_EQ(parts[1].type(), ElementType::span);
  EXPECT_EQ(parts[1].as_span().style().font_style, FontStyle::italic);
  EXPECT_EQ(parts[3].as_span().style().font_weight, FontWeight::bold);
  EXPECT_EQ(parts[5].as_span().style().font_line_through, true);
}

/// Nested emphasis nests spans, each carrying only its own mark.
TEST(MarkdownDocument, nested_emphasis_nests_spans) {
  const Document md = document("***bold italic***\n");
  const Element outer = children(children(root(md))[0])[0];
  const Element inner = children(outer)[0];

  EXPECT_EQ(outer.as_span().style().font_style, FontStyle::italic);
  EXPECT_FALSE(outer.as_span().style().font_weight.has_value());
  EXPECT_EQ(inner.as_span().style().font_weight, FontWeight::bold);
  EXPECT_EQ(text_of(outer), "bold italic");
}

TEST(MarkdownDocument, inline_code_is_a_monospace_span) {
  const Document md = document("a `b` c\n");
  const Element span = children(children(root(md))[0])[1];

  EXPECT_EQ(span.type(), ElementType::span);
  EXPECT_EQ(span.as_span().style().font_name, "monospace");
  EXPECT_EQ(text_of(span), "b");
}

/// A code block is one monospace paragraph per line: the model has no
/// pre-formatted block, and one paragraph would collapse the newlines.
TEST(MarkdownDocument, a_code_block_is_one_paragraph_per_line) {
  const Document md = document("```\none\n\n  three\n```\n");
  const Element group = children(root(md))[0];
  const std::vector<Element> lines = children(group);

  EXPECT_EQ(group.type(), ElementType::group);
  ASSERT_EQ(lines.size(), 3);
  EXPECT_EQ(text_of(lines[0]), "one");
  EXPECT_EQ(text_of(lines[1]), "");
  EXPECT_EQ(text_of(lines[2]), "  three");
  EXPECT_EQ(lines[0].as_paragraph().text_style().font_name, "monospace");
}

TEST(MarkdownDocument, a_link_carries_its_href) {
  const Document md = document("see [here](https://a.example/b).\n");
  const Element link = children(children(root(md))[0])[1];

  EXPECT_EQ(link.type(), ElementType::link);
  EXPECT_EQ(link.as_link().href(), "https://a.example/b");
  EXPECT_EQ(text_of(link), "here");
}

TEST(MarkdownDocument, an_unordered_list) {
  const Document md = document("- a\n- b\n");
  const Element list = children(root(md))[0];
  const std::vector<Element> items = children(list);

  EXPECT_EQ(list.as_list().type(), ListType::unordered);
  ASSERT_EQ(items.size(), 2);
  EXPECT_EQ(items[0].as_list_item().marker(), "•");
  EXPECT_FALSE(items[0].as_list_item().number().has_value());
  EXPECT_EQ(text_of(items[1]), "b");
}

TEST(MarkdownDocument, an_ordered_list_counts_from_its_start) {
  const Document md = document("3. a\n4. b\n");
  const Element list = children(root(md))[0];
  const std::vector<Element> items = children(list);

  EXPECT_EQ(list.as_list().type(), ListType::ordered);
  ASSERT_EQ(items.size(), 2);
  EXPECT_EQ(items[0].as_list_item().marker(), "3.");
  EXPECT_EQ(items[0].as_list_item().number(), 3);
  EXPECT_EQ(items[1].as_list_item().marker(), "4.");
  EXPECT_EQ(items[1].as_list_item().number(), 4);
}

/// The model has no checkbox, so the box replaces the item's marker.
TEST(MarkdownDocument, a_task_list_item_is_marked_with_its_box) {
  const Document md = document("- [ ] a\n- [x] b\n");
  const std::vector<Element> items = children(children(root(md))[0]);

  ASSERT_EQ(items.size(), 2);
  EXPECT_EQ(items[0].as_list_item().marker(), "☐");
  EXPECT_EQ(items[1].as_list_item().marker(), "☑");
}

/// A quote is a group — the renderer writes no wrapper for one — so what
/// survives is the left margin its paragraphs carry, one step per level.
TEST(MarkdownDocument, a_block_quote_indents_its_paragraphs) {
  const Document md = document("> one\n>\n> > two\n");
  const Element quote = children(root(md))[0];
  const std::vector<Element> inner = children(quote);

  EXPECT_EQ(quote.type(), ElementType::group);
  ASSERT_EQ(inner.size(), 2);

  const ParagraphStyle one = inner[0].as_paragraph().style();
  const ParagraphStyle two = children(inner[1])[0].as_paragraph().style();

  ASSERT_TRUE(one.margin.left.has_value());
  ASSERT_TRUE(two.margin.left.has_value());
  EXPECT_LT(one.margin.left->magnitude(), two.margin.left->magnitude());
}

TEST(MarkdownDocument, a_gfm_table) {
  const Document md = document("| a | b |\n"
                               "|---|--:|\n"
                               "| 1 | 2 |\n");
  const Element table = children(root(md))[0];

  ASSERT_EQ(table.type(), ElementType::table);
  EXPECT_EQ(table.as_table().dimensions().rows, 2);
  EXPECT_EQ(table.as_table().dimensions().columns, 2);

  std::vector<Element> rows;
  for (const Element row : table.as_table().rows()) {
    rows.push_back(row);
  }
  ASSERT_EQ(rows.size(), 2);

  const std::vector<Element> header = children(rows[0]);
  ASSERT_EQ(header.size(), 2);
  EXPECT_EQ(text_of(header[0]), "a");
  // a header cell is bold, on the span the renderer takes the weight from
  const Element header_span = children(children(header[0])[0])[0];
  EXPECT_EQ(header_span.as_span().style().font_weight, FontWeight::bold);
  EXPECT_EQ(header[1].as_table_cell().style().horizontal_align,
            HorizontalAlign::right);
  EXPECT_EQ(text_of(children(rows[1])[1]), "2");
}

TEST(MarkdownDocument, entities_are_resolved) {
  const Document md = document("&amp; &#65; &#x42; &notanentity;\n");

  EXPECT_EQ(text_of(root(md)), "& A B &notanentity;");
}

/// There is no passthrough element in the model, so raw html goes nowhere.
TEST(MarkdownDocument, raw_html_is_dropped) {
  const Document inline_html = document("a <b>bold</b> c\n");
  const Document block_html = document("<div>\nblock\n</div>\n");

  EXPECT_EQ(text_of(root(inline_html)), "a bold c");
  EXPECT_EQ(text_of(root(block_html)), "");
}

/// Stage 4: an image is not a frame yet, and its alt text is what is left.
TEST(MarkdownDocument, an_image_leaves_its_alt_text_behind) {
  const Document md = document("![a diagram](d.svg)\n");

  EXPECT_EQ(text_of(root(md)), "a diagram");
}
