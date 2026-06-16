#include <odr/internal/pdf/pdf_page_text.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;
using odr::Logger;
using odr::internal::util::math::Transform2D;

namespace {

std::vector<TextElement> run(const std::string &content,
                             const Resources &resources) {
  return extract_text(content, resources, Logger::null());
}

// Run a content stream with no font resources: fonts resolve to null, so the
// emitted `text` is the raw codes and advances are zero — lets us assert
// transform positioning in isolation.
std::vector<TextElement> run(const std::string &content) {
  Resources resources;
  return run(content, resources);
}

// A simple font `widths` (glyph space, 1/1000 em) starting at `first_char`.
Font simple_font(int first_char, std::vector<double> widths) {
  Font font;
  font.first_char = first_char;
  font.widths = std::move(widths);
  return font;
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

// `TJ` emits one element per string; with no font the strings stay put (zero
// advance) but the numeric adjustments still move the origin.
TEST(PdfPageText, tj_emits_per_string_with_adjustments) {
  // adjustment -120 -> +120/1000 * 10 = +1.2 between the two strings
  const auto texts = run("BT /F1 10 Tf 0 0 Td [(Ab) -120 (cd)] TJ ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[0].codes, "Ab");
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 0);
  EXPECT_EQ(texts[1].codes, "cd");
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 1.2);
}

// Simple-font `/Widths` advance the text matrix, so a following show lands past
// the previous one.
TEST(PdfPageText, simple_font_widths_advance) {
  Font font = simple_font('A', {500, 600, 700}); // A=0.5, B=0.6, C=0.7 em
  Resources res;
  res.font["F1"] = &font;

  const auto texts = run("BT /F1 10 Tf 0 0 Td (AB) Tj (C) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 0);
  EXPECT_DOUBLE_EQ(texts[0].width, 11); // 5 + 6
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 11);
  EXPECT_DOUBLE_EQ(texts[1].width, 7);
}

// A `TJ` adjustment combines with the glyph width to place the next string.
TEST(PdfPageText, tj_adjustment_after_width) {
  Font font = simple_font('A', {500, 600}); // A=0.5, B=0.6 em
  Resources res;
  res.font["F1"] = &font;

  // (A): width 5; -100 -> +1.0; (B) lands at 6.0
  const auto texts = run("BT /F1 10 Tf 0 0 Td [(A) -100 (B)] TJ ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 0);
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 6);
}

// Char spacing adds to every glyph's advance.
TEST(PdfPageText, char_spacing_advance) {
  Font font = simple_font('A', {500, 500});
  Resources res;
  res.font["F1"] = &font;

  // Tc=2: each of A,B advances 5 + 2 = 7 -> 14 total
  const auto texts = run("BT /F1 10 Tf 2 Tc 0 0 Td (AB) Tj (x) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].width, 14);
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 14);
}

// Word spacing adds to the single-byte space (0x20) only.
TEST(PdfPageText, word_spacing_applies_to_space) {
  Font font = simple_font(32, {250}); // space = 0.25 em
  Resources res;
  res.font["F1"] = &font;

  // Tw=5: the space advances 2.5 + 5 = 7.5
  const auto texts = run("BT /F1 10 Tf 5 Tw 0 0 Td ( ) Tj (x) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].width, 7.5);
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 7.5);
}

// Composite (Type0) fonts use 2-byte codes and the `/DW` default width.
TEST(PdfPageText, composite_default_width_advance) {
  Font font;
  font.composite = true;
  font.cid_default_width = 1000; // 1.0 em
  Resources res;
  res.font["F1"] = &font;

  // <0001> and <0002> are one 2-byte code each; size 10 -> advance 10 apiece
  const auto texts = run("BT /F1 10 Tf 0 0 Td <0001> Tj <0002> Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].width, 10);
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 10);
}

// `Font::advance_width` falls back to `/MissingWidth` (simple) and `/DW`
// (composite) for absent codes.
TEST(PdfPageText, advance_width_fallbacks) {
  Font simple = simple_font('A', {500});
  simple.missing_width = 250;
  EXPECT_DOUBLE_EQ(simple.advance_width('A'), 0.5);
  EXPECT_DOUBLE_EQ(simple.advance_width('B'), 0.25); // out of range

  Font composite;
  composite.composite = true;
  composite.cid_default_width = 1000;
  composite.cid_widths[1] = 2000;
  EXPECT_DOUBLE_EQ(composite.advance_width(1), 2.0);
  EXPECT_DOUBLE_EQ(composite.advance_width(2), 1.0); // default
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

namespace {

// A form XObject carrying an (already decoded) content stream.
XObject form_x_object(std::string content) {
  XObject x_object;
  x_object.subtype = XObject::Subtype::form;
  x_object.content = std::move(content);
  return x_object;
}

} // namespace

// `Do` on a form XObject runs its content stream and emits its text.
TEST(PdfPageText, form_xobject_invoked) {
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (Hi) Tj ET");
  Resources res;
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].codes, "Hi");
}

// The form `/Matrix` concatenates onto the CTM, placing the form's content.
TEST(PdfPageText, form_xobject_matrix_applies) {
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (X) Tj ET");
  form.matrix = Transform2D::translation(100, 200);
  Resources res;
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 100);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 200);
}

// The CTM (incl. the form matrix) is restored after the form, so following page
// content is unaffected.
TEST(PdfPageText, form_xobject_restores_state) {
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (a) Tj ET");
  form.matrix = Transform2D::translation(100, 0);
  Resources res;
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do BT /F1 10 Tf 5 0 Td (b) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 100); // shifted by the form matrix
  EXPECT_DOUBLE_EQ(texts[1].transform.e, 5);   // outside the form, not shifted
}

// A form resolves fonts against its own `/Resources`, not the invoking scope.
TEST(PdfPageText, form_xobject_uses_own_resources) {
  Font font = simple_font('A', {500});
  Resources form_res;
  form_res.font["F1"] = &font;
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (A) Tj ET");
  form.resources = &form_res;

  Resources res; // no F1 here
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].width, 5); // width resolved via the form's font
}

// Forms nest: a form may invoke another form (resolved in its own resources).
TEST(PdfPageText, form_xobject_nested) {
  XObject inner = form_x_object("BT /F1 10 Tf 0 0 Td (in) Tj ET");
  Resources inner_res;
  inner_res.x_object["Inner"] = &inner;
  XObject outer = form_x_object("/Inner Do");
  outer.resources = &inner_res;

  Resources res;
  res.x_object["Outer"] = &outer;

  const auto texts = run("/Outer Do", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].codes, "in");
}

// Image XObjects are recognized but not rendered (stage 4): `Do` is a no-op.
TEST(PdfPageText, image_xobject_ignored) {
  XObject image;
  image.subtype = XObject::Subtype::image;
  Resources res;
  res.x_object["Im0"] = &image;

  EXPECT_TRUE(run("/Im0 Do", res).empty());
}

// An unknown XObject name is skipped without throwing.
TEST(PdfPageText, unknown_xobject_ignored) {
  EXPECT_TRUE(run("/Missing Do").empty());
}

// A form that invokes itself (a cyclic graph, as the parser now represents it)
// terminates at render time: the body runs once, the re-entrant `Do` is
// skipped.
TEST(PdfPageText, form_xobject_self_cycle_terminates) {
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (a) Tj ET /Self Do");
  Resources form_res;
  form_res.x_object["Self"] = &form; // the form references itself
  form.resources = &form_res;

  Resources res;
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do", res);
  ASSERT_EQ(texts.size(), 1); // body emitted once, the cycle is cut
  EXPECT_EQ(texts[0].codes, "a");
}
