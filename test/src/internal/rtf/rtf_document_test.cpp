#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <odr/internal/common/file.hpp>
#include <odr/internal/rtf/rtf_element_registry.hpp>
#include <odr/internal/rtf/rtf_file.hpp>
#include <odr/internal/rtf/rtf_parser.hpp>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// The parsed tree as one line: `P(…)` per paragraph, `|` for a line break and
/// `PB` for a page break.
std::string flatten(const std::string &content) {
  rtf::ElementRegistry registry;
  std::istringstream in(content);
  const ElementIdentifier root = rtf::parse_tree(registry, in);

  std::string result;
  for (ElementIdentifier child = registry.element_at(root).first_child_id;
       child != null_element_id;
       child = registry.element_at(child).next_sibling_id) {
    if (registry.element_at(child).type == ElementType::page_break) {
      result += "PB";
      continue;
    }
    result += "P(";
    for (ElementIdentifier inner = registry.element_at(child).first_child_id;
         inner != null_element_id;
         inner = registry.element_at(inner).next_sibling_id) {
      switch (registry.element_at(inner).type) {
      case ElementType::text:
        result += registry.text_element_at(inner).text;
        break;
      case ElementType::line_break:
        result += "|";
        break;
      default:
        result += "?";
        break;
      }
    }
    result += ")";
  }
  return result;
}

std::shared_ptr<MemoryFile> memory_file(const std::string &content) {
  return std::make_shared<MemoryFile>(content);
}

} // namespace

TEST(RtfDocument, plain_paragraphs) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi Hello, World!\par})"), "P(Hello, World!)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi one\par two\par})"), "P(one)P(two)");
}

TEST(RtfDocument, a_trailing_paragraph_mark_adds_no_empty_paragraph) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi one\par})"), "P(one)");
  // an explicit empty paragraph is real content and stays
  EXPECT_EQ(flatten(R"({\rtf1\ansi one\par\par two\par})"), "P(one)P()P(two)");
}

TEST(RtfDocument, text_after_the_last_paragraph_mark) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi one\par two})"), "P(one)P(two)");
}

TEST(RtfDocument, line_break_tab_and_page_break) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\line b\par})"), "P(a|b)");
  EXPECT_EQ(flatten("{\\rtf1\\ansi a\\tab b\\par}"), "P(a\tb)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\page b\par})"), "P(a)PBP(b)");
}

TEST(RtfDocument,
     a_page_or_section_break_ends_a_paragraph_without_opening_one) {
  // `\par\page` is what a writer emits, and the page break must not blank-line
  // the document by fabricating a paragraph the paragraph mark already closed
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\par\page b\par})"), "P(a)PBP(b)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi\page a\par})"), "PBP(a)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\par\sect b\par})"), "P(a)P(b)");
}

TEST(RtfDocument, a_line_break_control_symbol_is_a_paragraph) {
  EXPECT_EQ(flatten("{\\rtf1\\ansi a\\\nb}"), "P(a)P(b)");
}

TEST(RtfDocument, a_document_with_no_body_is_empty) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi})"), "");
  EXPECT_EQ(flatten(R"({\rtf1\ansi{\fonttbl{\f0\fnil Arial;}}})"), "");
}

TEST(RtfDocument, character_control_words) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\emdash b\bullet\par})"), "P(a\xe2\x80\x94"
                                                              "b\xe2\x80\xa2)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi a\~b\par})"), "P(a\xc2\xa0"
                                                 "b)");
}

TEST(RtfDocument, unknown_control_words_are_ignored) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi\b\i\fs28 bold\b0  and not\par})"),
            "P(bold and not)");
}

TEST(RtfDocument, header_tables_are_not_body_text) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi\deff0)"
                    R"({\fonttbl{\f0\fnil Arial;}})"
                    R"({\colortbl;\red0\green0\blue0;})"
                    R"({\info{\title Ignored}})"
                    R"(Body\par})"),
            "P(Body)");
}

TEST(RtfDocument, ignorable_destinations_are_skipped) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi{\*\generator Writer;}Body\par})"),
            "P(Body)");
  // a field keeps its cached result and drops the instruction
  EXPECT_EQ(
      flatten(R"({\rtf1\ansi{\field{\*\fldinst PAGE }{\fldrslt 7}}\par})"),
      "P(7)");
}

TEST(RtfDocument, a_binary_payload_does_not_desync_the_groups) {
  EXPECT_EQ(flatten("{\\rtf1\\ansi{\\*\\x\\bin2 }}}Hello\\par}"), "P(Hello)");
}

TEST(RtfDocument, an_unmatched_group_close_is_ignored) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi a}}b\par})"), "P(ab)");
}

TEST(RtfDocument, a_group_left_open_throws) {
  EXPECT_THROW(flatten(R"({\rtf1\ansi Hello)"), std::runtime_error);
}

TEST(RtfDocument, hex_escapes_use_the_run_encoding) {
  // windows-1252 by default
  EXPECT_EQ(flatten(R"({\rtf1\ansi \'e4\par})"), "P(\xc3\xa4)");
  // and `\ansicpgN` overrides it
  EXPECT_EQ(flatten(R"({\rtf1\ansi\ansicpg1251 \'e4\par})"), "P(\xd0\xb4)");
}

TEST(RtfDocument, the_other_encoding_selectors) {
  // `\pc` and `\pca` are IBM 437 / 850, which `internal/encoding` has no row
  // for, so they fall back to windows-1252 — as does an unknown code page
  EXPECT_EQ(flatten(R"({\rtf1\pc \'e4\par})"), "P(\xc3\xa4)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi\ansicpg1 \'e4\par})"), "P(\xc3\xa4)");
  EXPECT_EQ(flatten(R"({\rtf1\mac \'d5\par})"), "P(\xe2\x80\x99)");
}

TEST(RtfDocument, an_undecodable_encoding_degrades_per_byte) {
  // shift_jis is named but not decoded, so the ascii skeleton survives and
  // only the bytes of the cjk character become U+FFFD
  EXPECT_EQ(flatten(R"({\rtf1\ansi\ansicpg932 a\'82\'a0b\par})"),
            "P(a\xef\xbf\xbd\xef\xbf\xbd"
            "b)");
}

TEST(RtfDocument, unicode_escapes) {
  // the parameter is signed, so U+F020 arrives as -4064
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc1 \u-4064 ?\par})"), "P(\xef\x80\xa0)");
  // a non-bmp character is a surrogate pair, each half with its own fallback
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc1 \u-10179 ?\u-8704 ?\par})"),
            "P(\xf0\x9f\x98\x80)");
  // an unpaired high surrogate is U+FFFD
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc1 \u-10179 ?A\par})"), "P(\xef\xbf\xbd"
                                                            "A)");
}

TEST(RtfDocument, a_zero_code_point_is_dropped_rather_than_written) {
  // a NUL byte would travel into the html verbatim; the parameterless `\u`
  // folds to 0 as well, and its fallback character is still skipped
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc0\u0 A\par})"), "P(A)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi\u X\par})"), "P()");
}

TEST(RtfDocument, uc_skips_a_control_word_as_one_character) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc1 \u65 \tab X\par})"), "P(AX)");
  // a `\binN` and its payload count as one character together
  EXPECT_EQ(flatten("{\\rtf1\\ansi\\uc1 \\u65 \\bin2 xyZ\\par}"), "P(AZ)");
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc2 \u65 ??X\par})"), "P(AX)");
  // `\ucN` is a character property, so a group restores it
  EXPECT_EQ(flatten(R"({\rtf1\ansi\uc1 {\uc0 \u65 }\u66 ?X\par})"), "P(ABX)");
}

TEST(RtfDocument, table_rows_read_as_paragraphs) {
  EXPECT_EQ(flatten(R"({\rtf1\ansi\trowd a\cell b\cell\row c\par})"),
            "P(a\tb\t)P(c)");
}

TEST(RtfDocument, opens_through_the_file_layer) {
  const rtf::RtfFile file(memory_file(R"({\rtf1\ansi Hello\par})"));

  EXPECT_EQ(file.file_type(), FileType::rich_text_format);
  EXPECT_EQ(file.document_type(), DocumentType::text);
  EXPECT_TRUE(file.is_decodable());

  const Document document(file.document());
  EXPECT_EQ(document.file_type(), FileType::rich_text_format);
  EXPECT_EQ(document.document_type(), DocumentType::text);

  const Element root = document.root_element();
  ASSERT_TRUE(root);
  EXPECT_EQ(root.type(), ElementType::root);
}

TEST(RtfDocument, something_else_is_no_rtf_file) {
  EXPECT_THROW(rtf::RtfFile(memory_file("Hello, World!")), NoRtfFile);
}

TEST(RtfDocument, the_open_strategy_opens_an_rtf) {
  const File file(memory_file(R"({\rtf1\ansi Hello\par})"));

  // magic names the type and the strategy decodes it
  const DecodedFile detected(file);
  EXPECT_EQ(detected.file_type(), FileType::rich_text_format);
  EXPECT_TRUE(detected.is_document_file());

  // as does asking for the type outright
  EXPECT_EQ(DecodedFile(file, FileType::rich_text_format).file_type(),
            FileType::rich_text_format);
  // the branch's `NoRtfFile` is what `open_file` catches to move on to the
  // next candidate type, so a caller asking for an rtf that is not one sees
  // the strategy's own answer
  EXPECT_THROW(DecodedFile(File(memory_file("Hello, World!")),
                           FileType::rich_text_format),
               UnknownFileType);

  // and the document-file path, which a caller reaches through `DocumentFile`
  const DocumentFile document_file(file);
  EXPECT_EQ(document_file.file_type(), FileType::rich_text_format);
  EXPECT_EQ(document_file.document_type(), DocumentType::text);
}

TEST(RtfDocument, the_file_type_table_reports_a_text_document) {
  EXPECT_EQ(document_type_by_file_type(FileType::rich_text_format),
            DocumentType::text);

  const FileTypeCapabilities capabilities =
      capabilities_by_file_type(FileType::rich_text_format);
  EXPECT_TRUE(capabilities.detect_by_content);
  EXPECT_TRUE(capabilities.open);
  EXPECT_TRUE(capabilities.translate_html);
  EXPECT_TRUE(capabilities.color_scheme);
}

TEST(RtfDocument, translates_to_html) {
  const auto file = std::make_shared<rtf::RtfFile>(
      memory_file(R"({\rtf1\ansi Hello, World!\par})"));

  std::ostringstream out;
  html::translate(DecodedFile(file), {}).list_views().at(0).write_html(out);

  EXPECT_NE(out.str().find("Hello, World!"), std::string::npos);
}

TEST(RtfDocument, an_empty_document_renders) {
  const auto file =
      std::make_shared<rtf::RtfFile>(memory_file(R"({\rtf1\ansi})"));

  std::ostringstream out;
  EXPECT_NO_THROW(html::translate(DecodedFile(file), {})
                      .list_views()
                      .at(0)
                      .write_html(out));
}
