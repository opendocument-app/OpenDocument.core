#include <odr/internal/pdf/pdf_annotation.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_writer.hpp>

#include <test_util.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

std::string mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R /ID [<0102> <0304>]");
  return builder.build_classic();
}

const Page *first_page(const Document &document) {
  const auto &kids = document.catalog->pages->kids;
  return kids.empty() ? nullptr : dynamic_cast<Page *>(kids.front());
}

/// Writes `annotate`'s annotations onto the first page of `pdf` and returns the
/// resulting file.
template <typename Annotate>
std::string annotated(const std::string &pdf, Annotate &&annotate) {
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();
  const Page *page = first_page(*document);
  EXPECT_NE(page, nullptr);

  IncrementalWriter writer(parser);
  append_page_annotations(writer, *page, annotate(writer));
  std::ostringstream out;
  writer.write(out);
  return std::move(out).str();
}

TextMarkup one_line_highlight() {
  TextMarkup markup;
  markup.kind = TextMarkupKind::highlight;
  markup.quads.push_back({72, 700, 300, 700, 72, 688, 300, 688});
  markup.common.color = {1, 0.9, 0.2};
  return markup;
}

} // namespace

// The annotation and its appearance both land, and the page reaches them.
TEST(PdfAnnotation, highlight_round_trips) {
  const std::string result = annotated(mini_pdf(), [](IncrementalWriter &w) {
    return std::vector{write_text_markup(w, one_line_highlight())};
  });

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  const std::unique_ptr<Document> document = parser.parse_document();
  const Page *page = first_page(*document);
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->annotations.size(), 1);

  const Annotation &annotation = *page->annotations.front();
  const Dictionary &dictionary = annotation.object.as_dictionary();
  EXPECT_EQ(dictionary.get("Subtype").as_string(), "Highlight");
  EXPECT_EQ(dictionary.get("QuadPoints").as_array().size(), 8);
  EXPECT_EQ(dictionary.get("F").as_integer(), 4);
  EXPECT_EQ(dictionary.get("NM").as_string(), "odr-6");

  // `/Rect` is the union of the quads
  const std::vector<double> rect = dictionary.get("Rect").as_reals();
  EXPECT_DOUBLE_EQ(rect[0], 72);
  EXPECT_DOUBLE_EQ(rect[1], 688);
  EXPECT_DOUBLE_EQ(rect[2], 300);
  EXPECT_DOUBLE_EQ(rect[3], 700);

  // the parser resolved `/AP /N` into a form, which is what makes it paint
  ASSERT_NE(annotation.appearance, nullptr);
}

// 11.6.4.1: the highlight is a wash, so its appearance multiplies; the marks
// drawn on top of the text do not.
TEST(PdfAnnotation, only_highlight_multiplies) {
  const auto blend_mode = [](const TextMarkupKind kind) {
    TextMarkup markup = one_line_highlight();
    markup.kind = kind;

    const std::string result =
        annotated(mini_pdf(), [&markup](IncrementalWriter &w) {
          return std::vector{write_text_markup(w, markup)};
        });

    DocumentParser parser(std::make_unique<std::istringstream>(result));
    const std::unique_ptr<Document> document = parser.parse_document();
    const Annotation &annotation = *first_page(*document)->annotations.front();
    const ObjectReference appearance = annotation.object.as_dictionary()
                                           .get("AP")
                                           .as_dictionary()
                                           .get("N")
                                           .as_reference();
    const Dictionary &state = parser.read_object(appearance)
                                  .object.as_dictionary()
                                  .get("Resources")
                                  .as_dictionary()
                                  .get("ExtGState")
                                  .as_dictionary()
                                  .get("G0")
                                  .as_dictionary();
    return state.has_key("BM") ? state.get("BM").as_string() : std::string();
  };

  EXPECT_EQ(blend_mode(TextMarkupKind::highlight), "Multiply");
  EXPECT_EQ(blend_mode(TextMarkupKind::underline), "");
  EXPECT_EQ(blend_mode(TextMarkupKind::strike_out), "");
  EXPECT_EQ(blend_mode(TextMarkupKind::squiggly), "");
}

TEST(PdfAnnotation, text_markup_subtypes) {
  const auto subtype = [](const TextMarkupKind kind) {
    TextMarkup markup = one_line_highlight();
    markup.kind = kind;
    const std::string result =
        annotated(mini_pdf(), [&markup](IncrementalWriter &w) {
          return std::vector{write_text_markup(w, markup)};
        });
    DocumentParser parser(std::make_unique<std::istringstream>(result));
    const std::unique_ptr<Document> document = parser.parse_document();
    return first_page(*document)
        ->annotations.front()
        ->object.as_dictionary()
        .get("Subtype")
        .as_string();
  };

  EXPECT_EQ(subtype(TextMarkupKind::highlight), "Highlight");
  EXPECT_EQ(subtype(TextMarkupKind::underline), "Underline");
  EXPECT_EQ(subtype(TextMarkupKind::strike_out), "StrikeOut");
  EXPECT_EQ(subtype(TextMarkupKind::squiggly), "Squiggly");
}

TEST(PdfAnnotation, ink_round_trips) {
  Ink ink;
  ink.strokes.push_back({100, 500, 130, 540, 160, 490});
  ink.width = 2;
  ink.common.color = {0.9, 0.1, 0.1};

  const std::string result =
      annotated(mini_pdf(), [&ink](IncrementalWriter &w) {
        return std::vector{write_ink(w, ink)};
      });

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  const std::unique_ptr<Document> document = parser.parse_document();
  const Page *page = first_page(*document);
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->annotations.size(), 1);

  const Annotation &annotation = *page->annotations.front();
  const Dictionary &dictionary = annotation.object.as_dictionary();
  EXPECT_EQ(dictionary.get("Subtype").as_string(), "Ink");
  ASSERT_EQ(dictionary.get("InkList").as_array().size(), 1);
  EXPECT_EQ(dictionary.get("InkList").as_array()[0].as_array().size(), 6);
  EXPECT_DOUBLE_EQ(dictionary.get("BS").as_dictionary().get("W").as_real(), 2);
  ASSERT_NE(annotation.appearance, nullptr);

  // the box is grown by the stroke width, which straddles the path
  const std::vector<double> rect = dictionary.get("Rect").as_reals();
  EXPECT_DOUBLE_EQ(rect[0], 98);
  EXPECT_DOUBLE_EQ(rect[1], 488);
  EXPECT_DOUBLE_EQ(rect[2], 162);
  EXPECT_DOUBLE_EQ(rect[3], 542);
}

TEST(PdfAnnotation, empty_geometry_throws) {
  DocumentParser parser(std::make_unique<std::istringstream>(mini_pdf()));
  IncrementalWriter writer(parser);

  EXPECT_THROW(std::ignore = write_text_markup(writer, TextMarkup{}),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = write_ink(writer, Ink{}), std::invalid_argument);

  Ink odd;
  odd.strokes.push_back({1, 2, 3});
  EXPECT_THROW(std::ignore = write_ink(writer, odd), std::invalid_argument);
}

TEST(PdfAnnotation, optional_fields_are_omitted_when_empty) {
  const auto annotation_of = [](const TextMarkup &markup, DocumentParser &out) {
    return first_page(*out.parse_document())
        ->annotations.front()
        ->object.as_dictionary();
  };

  TextMarkup markup = one_line_highlight();
  {
    DocumentParser parser(std::make_unique<std::istringstream>(
        annotated(mini_pdf(), [&markup](IncrementalWriter &w) {
          return std::vector{write_text_markup(w, markup)};
        })));
    const Dictionary dictionary = annotation_of(markup, parser);
    EXPECT_FALSE(dictionary.has_key("T"));
    EXPECT_FALSE(dictionary.has_key("Contents"));
  }

  markup.common.author = "a reviewer";
  markup.common.contents = "why (this) matters";
  {
    DocumentParser parser(std::make_unique<std::istringstream>(
        annotated(mini_pdf(), [&markup](IncrementalWriter &w) {
          return std::vector{write_text_markup(w, markup)};
        })));
    const Dictionary dictionary = annotation_of(markup, parser);
    EXPECT_EQ(dictionary.get("T").as_string(), "a reviewer");
    // the parenthesis survives the escaping the writer applies
    EXPECT_EQ(dictionary.get("Contents").as_string(), "why (this) matters");
  }
}

// Several annotations on one page share a single page rewrite.
TEST(PdfAnnotation, appends_several_annotations) {
  Ink ink;
  ink.strokes.push_back({10, 10, 20, 20});

  const std::string result =
      annotated(mini_pdf(), [&ink](IncrementalWriter &w) {
        return std::vector{write_text_markup(w, one_line_highlight()),
                           write_ink(w, ink)};
      });

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  const std::unique_ptr<Document> document = parser.parse_document();
  EXPECT_EQ(first_page(*document)->annotations.size(), 2);
}

// A real file whose pages already carry link annotations: ours are appended,
// the existing ones stay.
TEST(PdfAnnotation, appends_to_an_existing_annots_array) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  std::size_t before = 0;
  std::ostringstream out;
  {
    DocumentParser parser(file->stream());
    const std::unique_ptr<Document> document = parser.parse_document();
    const Page *page = first_page(*document);
    ASSERT_NE(page, nullptr);
    before = page->annotations.size();
    ASSERT_GT(before, 0);

    IncrementalWriter writer(parser);
    append_page_annotations(writer, *page,
                            {write_text_markup(writer, one_line_highlight())});
    writer.write(out);
  }

  DocumentParser parser(
      std::make_unique<std::istringstream>(std::move(out).str()));
  const std::unique_ptr<Document> document = parser.parse_document();
  const Page *page = first_page(*document);
  ASSERT_NE(page, nullptr);
  EXPECT_EQ(page->annotations.size(), before + 1);
  EXPECT_NE(page->annotations.back()->appearance, nullptr);
}
