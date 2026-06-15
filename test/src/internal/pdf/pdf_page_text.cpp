#include <odr/internal/pdf/pdf_page_text.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;
using odr::Logger;

namespace {

// Run a content stream with no font resources: fonts resolve to null, so the
// emitted `text` is the raw codes and we can assert positioning in isolation.
std::vector<TextElement> run(const std::string &content) {
  Resources resources;
  return extract_text(content, resources, Logger::null());
}

} // namespace

// `Td` places the origin via the text line matrix; the font size is carried
// separately, not folded into the transform.
TEST(PdfPageText, td_translation) {
  const auto texts = run("BT /F1 12 Tf 100 700 Td (Hi) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 100);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 700);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 1);
  EXPECT_DOUBLE_EQ(texts[0].size, 12);
  EXPECT_EQ(texts[0].codes, "Hi");
  EXPECT_EQ(texts[0].text, "Hi"); // no font -> raw codes pass through
}

// `Tm` sets the text matrix outright, scaling and all.
TEST(PdfPageText, tm_scaling) {
  const auto texts = run("BT /F1 10 Tf 2 0 0 2 50 60 Tm (X) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 50);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 60);
}

// `cm` concatenates into the CTM, which composes under the text matrix.
TEST(PdfPageText, ctm_concatenates) {
  const auto texts = run("2 0 0 2 0 0 cm BT /F1 10 Tf 10 20 Td (Y) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 20); // 10 * 2
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 40); // 20 * 2
}

// Horizontal scaling (`Tz`, percent) scales x only, in the transform.
TEST(PdfPageText, horizontal_scaling) {
  const auto texts = run("BT /F1 10 Tf 50 Tz 0 0 Td (Z) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 0.5);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 1);
  EXPECT_DOUBLE_EQ(texts[0].horizontal_scaling, 50);
}

// Text rise (`Ts`) offsets the origin in y, unscaled by the font size.
TEST(PdfPageText, text_rise) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td 5 Ts (R) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 5);
  EXPECT_DOUBLE_EQ(texts[0].rise, 5);
}

// `TJ` concatenates its strings; the numeric adjustments are dropped
// (stage 2.2).
TEST(PdfPageText, tj_concatenates_strings) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td [(Ab) -120 (cd)] TJ ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].codes, "Abcd");
}

// `T*` moves down one line by the leading set with `TL`.
TEST(PdfPageText, next_line_uses_leading) {
  const auto texts = run("BT /F1 10 Tf 14 TL 0 800 Td (a) Tj T* (b) Tj ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 800);
  EXPECT_DOUBLE_EQ(texts[1].transform.f, 786); // 800 - 14
}

// `'` does the line move and then shows.
TEST(PdfPageText, show_next_line) {
  const auto texts = run("BT /F1 10 Tf 10 TL 0 500 Td (a) Tj (b) ' ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[1].transform.f, 490); // 500 - 10
  EXPECT_EQ(texts[1].codes, "b");
}

// `"` sets word/char spacing, does the line move, then shows the third operand.
TEST(PdfPageText, show_next_line_set_spacing) {
  const auto texts = run("BT /F1 10 Tf 12 TL 0 400 Td (a) Tj 1 2 (b) \" ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[1].transform.f, 388); // 400 - 12
  EXPECT_EQ(texts[1].codes, "b");
  EXPECT_DOUBLE_EQ(texts[1].word_spacing, 1);
  EXPECT_DOUBLE_EQ(texts[1].char_spacing, 2);
}
