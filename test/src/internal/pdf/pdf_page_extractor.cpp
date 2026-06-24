#include <odr/internal/pdf/pdf_page_extractor.hpp>

#include <odr/logger.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>

#include <string>
#include <variant>
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
TEST(PdfPageExtractor, td_translation) {
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
TEST(PdfPageExtractor, tm_scaling) {
  const auto texts = run("BT /F1 10 Tf 2 0 0 2 50 60 Tm (X) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 50);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 60);
}

// `cm` concatenates into the CTM, which composes under the text matrix.
TEST(PdfPageExtractor, ctm_concatenates) {
  const auto texts = run("2 0 0 2 0 0 cm BT /F1 10 Tf 10 20 Td (Y) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.e, 20); // 10 * 2
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 40); // 20 * 2
}

// Horizontal scaling (`Tz`, percent) scales x only, in the transform.
TEST(PdfPageExtractor, horizontal_scaling) {
  const auto texts = run("BT /F1 10 Tf 50 Tz 0 0 Td (Z) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.a, 0.5);
  EXPECT_DOUBLE_EQ(texts[0].transform.d, 1);
  EXPECT_DOUBLE_EQ(texts[0].horizontal_scaling, 50);
}

// Text rise (`Ts`) offsets the origin in y, unscaled by the font size.
TEST(PdfPageExtractor, text_rise) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td 5 Ts (R) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 5);
  EXPECT_DOUBLE_EQ(texts[0].rise, 5);
}

// `TJ` emits one element per string; with no font the strings stay put (zero
// advance) but the numeric adjustments still move the origin.
TEST(PdfPageExtractor, tj_emits_per_string_with_adjustments) {
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
TEST(PdfPageExtractor, simple_font_widths_advance) {
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
TEST(PdfPageExtractor, tj_adjustment_after_width) {
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
TEST(PdfPageExtractor, char_spacing_advance) {
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
TEST(PdfPageExtractor, word_spacing_applies_to_space) {
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
TEST(PdfPageExtractor, composite_default_width_advance) {
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
TEST(PdfPageExtractor, advance_width_fallbacks) {
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
TEST(PdfPageExtractor, next_line_uses_leading) {
  const auto texts = run("BT /F1 10 Tf 14 TL 0 800 Td (a) Tj T* (b) Tj ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[0].transform.f, 800);
  EXPECT_DOUBLE_EQ(texts[1].transform.f, 786); // 800 - 14
}

// `'` does the line move and then shows.
TEST(PdfPageExtractor, show_next_line) {
  const auto texts = run("BT /F1 10 Tf 10 TL 0 500 Td (a) Tj (b) ' ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_DOUBLE_EQ(texts[1].transform.f, 490); // 500 - 10
  EXPECT_EQ(texts[1].codes, "b");
}

// `"` sets word/char spacing, does the line move, then shows the third operand.
TEST(PdfPageExtractor, show_next_line_set_spacing) {
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
TEST(PdfPageExtractor, form_xobject_invoked) {
  XObject form = form_x_object("BT /F1 10 Tf 0 0 Td (Hi) Tj ET");
  Resources res;
  res.x_object["Fm0"] = &form;

  const auto texts = run("/Fm0 Do", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].codes, "Hi");
}

// The form `/Matrix` concatenates onto the CTM, placing the form's content.
TEST(PdfPageExtractor, form_xobject_matrix_applies) {
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
TEST(PdfPageExtractor, form_xobject_restores_state) {
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
TEST(PdfPageExtractor, form_xobject_uses_own_resources) {
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
TEST(PdfPageExtractor, form_xobject_nested) {
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

// Image XObjects are recognized but not rendered: `Do` is a no-op.
TEST(PdfPageExtractor, image_xobject_ignored) {
  XObject image;
  image.subtype = XObject::Subtype::image;
  Resources res;
  res.x_object["Im0"] = &image;

  EXPECT_TRUE(run("/Im0 Do", res).empty());
}

// An unknown XObject name is skipped without throwing.
TEST(PdfPageExtractor, unknown_xobject_ignored) {
  EXPECT_TRUE(run("/Missing Do").empty());
}

// A form that invokes itself (a cyclic graph, as the parser now represents it)
// terminates at render time: the body runs once, the re-entrant `Do` is
// skipped.
TEST(PdfPageExtractor, form_xobject_self_cycle_terminates) {
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

// The text rendering mode (`Tr`) rides along on each element; the HTML layer
// turns the unpainted modes (3 invisible, 7 clip-only) into transparent but
// still selectable spans.
TEST(PdfPageExtractor, rendering_mode_propagates) {
  const auto texts = run("BT /F1 10 Tf 3 Tr 0 0 Td (x) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].rendering_mode, 3);
}

// A composite font with no `/ToUnicode` and no usable predefined encoding has
// no recoverable Unicode: the segment is marked `no_unicode` with empty text
// (the glyphs render once the embedded font lands in stage 3).
TEST(PdfPageExtractor, no_unicode_marks_composite_without_tounicode) {
  Font font;
  font.composite = true; // 2-byte codes, empty cmap, no cid_encoding_name
  Resources res;
  res.font["F1"] = &font;

  const auto texts = run("BT /F1 10 Tf 0 0 Td <0001> Tj ET", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_TRUE(texts[0].text.empty());
  EXPECT_TRUE(texts[0].no_unicode);
  EXPECT_EQ(texts[0].codes, std::string("\x00\x01", 2));
}

// `/ActualText` on a marked-content sequence overrides the per-glyph text for
// extraction (ligatures, reordered glyphs); a literal string is taken as-is.
TEST(PdfPageExtractor, actual_text_overrides_segment) {
  const auto texts =
      run("BT /F1 10 Tf 0 0 Td /Span <</ActualText (OK)>> BDC (xyz) Tj EMC ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "OK");
  EXPECT_EQ(texts[0].codes, "xyz"); // glyphs unchanged, only the text differs
  EXPECT_FALSE(texts[0].no_unicode);
}

// A UTF-16BE `/ActualText` (the common form, opening with the FE FF BOM) is
// decoded to UTF-8.
TEST(PdfPageExtractor, actual_text_utf16be) {
  // <FEFF 0041 0042> = "AB"
  const auto texts = run("BT /F1 10 Tf 0 0 Td /Span <</ActualText "
                         "<FEFF00410042>>> BDC (zz) Tj EMC ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "AB");
}

// A non-BOM `/ActualText` is PDFDocEncoding, not Latin-1: the upper-half bytes
// 0x83/0x84 decode to ellipsis/em dash, not the C1 controls Latin-1 would give.
TEST(PdfPageExtractor, actual_text_pdfdocencoding) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td /Span <</ActualText "
                         "<8384>>> BDC (zz) Tj EMC ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "…—"); // U+2026 U+2014
}

// `/ActualText` covers the whole sequence: it is emitted once, and the
// remaining shows in the sequence carry no extractable text of their own.
TEST(PdfPageExtractor, actual_text_emitted_once_then_suppressed) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td /Span <</ActualText (Sum)>> BDC "
                         "(a) Tj (b) Tj EMC ET");
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[0].text, "Sum");
  EXPECT_TRUE(texts[1].text.empty());
}

// A named property list (`BDC /Tag /Name`) resolves `/ActualText` through the
// `/Properties` resource subdictionary.
TEST(PdfPageExtractor, actual_text_named_property) {
  Dictionary props;
  props["ActualText"] = Object(StandardString("Z"));
  Resources res;
  res.properties["MC0"] = Object(props);

  const auto texts =
      run("BT /F1 10 Tf 0 0 Td /Span /MC0 BDC (q) Tj EMC ET", res);
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "Z");
}

// Marked content without `/ActualText` (a plain `BMC`, or a `BDC` whose
// property list carries none) leaves extraction untouched.
TEST(PdfPageExtractor, marked_content_without_actual_text_passthrough) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td /Tag BMC (hi) Tj EMC ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "hi"); // no font -> raw codes, no override
}

// A stray `EMC` (more ends than begins) is tolerated, not a crash.
TEST(PdfPageExtractor, stray_emc_tolerated) {
  const auto texts = run("BT /F1 10 Tf 0 0 Td EMC (ok) Tj ET");
  ASSERT_EQ(texts.size(), 1);
  EXPECT_EQ(texts[0].text, "ok");
}

// --- Stage 2.5: space inference ---

// A forward gap past ~0.2 em between segments on a line infers a space, so a
// producer that emits no space glyph still yields separable words.
TEST(PdfPageExtractor, space_inferred_on_word_gap) {
  Font font = simple_font('A', {500, 500}); // A, B = 0.5 em
  Resources res;
  res.font["F1"] = &font;

  // (A) ends at x=5; (B) starts at x=8 -> gap 3 > 0.2 * 10 em -> space
  const auto texts = run("BT /F1 10 Tf 0 0 Td (A) Tj 8 0 Td (B) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[0].text, "A");
  EXPECT_EQ(texts[1].text, " B");
}

// Abutting segments (the next begins where the last ended) get no space.
TEST(PdfPageExtractor, no_space_when_abutting) {
  Font font = simple_font('A', {500, 500});
  Resources res;
  res.font["F1"] = &font;

  const auto texts = run("BT /F1 10 Tf 0 0 Td (A) Tj (B) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[1].text, "B");
}

// A small `TJ` kern stays below threshold (no space); a large one crosses it.
TEST(PdfPageExtractor, tj_kern_threshold) {
  Font font = simple_font('A', {500, 500});
  Resources res;
  res.font["F1"] = &font;

  // -50 -> +0.5 gap, below 0.2 em
  EXPECT_EQ(run("BT /F1 10 Tf 0 0 Td [(A) -50 (B)] TJ ET", res)[1].text, "B");
  // -400 -> +4 gap, above 0.2 em
  EXPECT_EQ(run("BT /F1 10 Tf 0 0 Td [(A) -400 (B)] TJ ET", res)[1].text, " B");
}

// A perpendicular jump (a new text line) infers a space so lines don't merge.
TEST(PdfPageExtractor, space_inferred_on_new_line) {
  Font font = simple_font('A', {500, 500});
  Resources res;
  res.font["F1"] = &font;

  const auto texts = run("BT /F1 10 Tf 12 TL 0 20 Td (A) Tj T* (B) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[1].text, " B");
}

// A suppressed segment (the tail of an `/ActualText` span, a `no_unicode`
// glyph) carries no extractable text, but must not clear the trailing space of
// the segment before it, or the next gap would infer a second space.
TEST(PdfPageExtractor, trailing_space_survives_suppressed_segment) {
  Font font = simple_font('A', {500, 500}); // A, B = 0.5 em
  Resources res;
  res.font["F1"] = &font;

  // "A " is emitted once for the span; (B) is suppressed (empty). Despite the
  // 50-unit gap before the following show, no extra space is inferred.
  const auto texts = run("BT /F1 10 Tf 0 0 Td /Span <</ActualText (A )>> BDC "
                         "(A) Tj (B) Tj EMC 50 0 Td (A) Tj ET",
                         res);
  ASSERT_EQ(texts.size(), 3);
  EXPECT_EQ(texts[0].text, "A ");
  EXPECT_TRUE(texts[1].text.empty());
  EXPECT_EQ(texts[2].text, "A");
}

// A segment whose text already ends in a space suppresses the inferred one.
TEST(PdfPageExtractor, no_double_space_after_trailing_space) {
  Font font = simple_font('A', {500, 500}); // 'A'=5, the space falls back to 0
  Resources res;
  res.font["F1"] = &font;

  // "A " ends in a space; despite the big gap before (B), none is inferred.
  const auto texts = run("BT /F1 10 Tf 0 0 Td (A ) Tj 50 0 Td (B) Tj ET", res);
  ASSERT_EQ(texts.size(), 2);
  EXPECT_EQ(texts[0].text, "A ");
  EXPECT_EQ(texts[1].text, "B");
}

namespace {

std::vector<PageElement> run_page(const std::string &content) {
  Resources resources;
  return extract_page(content, resources, Logger::null());
}

const PathElement &path_at(const std::vector<PageElement> &page,
                           std::size_t index) {
  return std::get<PathElement>(page.at(index));
}

} // namespace

// A move/line/stroke builds one open subpath; the points are in user space and
// the paint intent is stroke-only.
TEST(PdfPageExtractor, path_move_line_stroke) {
  const auto page = run_page("100 200 m 300 400 l S");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_TRUE(p.stroke);
  EXPECT_FALSE(p.fill);
  ASSERT_EQ(p.subpaths.size(), 1);
  const Subpath &s = p.subpaths[0];
  EXPECT_FALSE(s.closed);
  EXPECT_DOUBLE_EQ(s.start[0], 100);
  EXPECT_DOUBLE_EQ(s.start[1], 200);
  ASSERT_EQ(s.segments.size(), 1);
  EXPECT_EQ(s.segments[0].kind, PathSegment::Kind::line);
  EXPECT_DOUBLE_EQ(s.segments[0].end[0], 300);
  EXPECT_DOUBLE_EQ(s.segments[0].end[1], 400);
}

// `re` appends a closed rectangle subpath; `f` fills it with nonzero winding.
TEST(PdfPageExtractor, path_rectangle_fill_nonzero) {
  const auto page = run_page("10 20 30 40 re f");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_TRUE(p.fill);
  EXPECT_FALSE(p.stroke);
  EXPECT_FALSE(p.even_odd);
  ASSERT_EQ(p.subpaths.size(), 1);
  const Subpath &s = p.subpaths[0];
  EXPECT_TRUE(s.closed);
  EXPECT_DOUBLE_EQ(s.start[0], 10);
  EXPECT_DOUBLE_EQ(s.start[1], 20);
  ASSERT_EQ(s.segments.size(), 3);
  EXPECT_DOUBLE_EQ(s.segments[0].end[0], 40); // 10 + 30
  EXPECT_DOUBLE_EQ(s.segments[0].end[1], 20);
  EXPECT_DOUBLE_EQ(s.segments[2].end[0], 10);
  EXPECT_DOUBLE_EQ(s.segments[2].end[1], 60); // 20 + 40
}

// `f*` selects the even-odd fill rule.
TEST(PdfPageExtractor, path_fill_evenodd) {
  const auto page = run_page("0 0 10 10 re f*");
  ASSERT_EQ(page.size(), 1);
  EXPECT_TRUE(path_at(page, 0).even_odd);
}

// `b` closes the open subpath, then fills and strokes it.
TEST(PdfPageExtractor, path_close_fill_stroke) {
  const auto page = run_page("0 0 m 10 0 l 10 10 l b");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_TRUE(p.fill);
  EXPECT_TRUE(p.stroke);
  ASSERT_EQ(p.subpaths.size(), 1);
  EXPECT_TRUE(p.subpaths[0].closed);
}

// The CTM in force at construction maps the points into user space.
TEST(PdfPageExtractor, path_points_under_ctm) {
  const auto page = run_page("2 0 0 2 5 5 cm 10 10 m 20 10 l S");
  ASSERT_EQ(page.size(), 1);
  const Subpath &s = path_at(page, 0).subpaths[0];
  EXPECT_DOUBLE_EQ(s.start[0], 25); // 10*2 + 5
  EXPECT_DOUBLE_EQ(s.start[1], 25);
  EXPECT_DOUBLE_EQ(s.segments[0].end[0], 45); // 20*2 + 5
  EXPECT_DOUBLE_EQ(s.segments[0].end[1], 25);
}

// `c` is a cubic Bézier carrying both control points; `v` and `y` derive one.
TEST(PdfPageExtractor, path_cubic_beziers) {
  const auto page = run_page("0 0 m 1 1 2 2 3 3 c 4 4 5 5 v 6 6 7 7 y S");
  ASSERT_EQ(page.size(), 1);
  const Subpath &s = path_at(page, 0).subpaths[0];
  ASSERT_EQ(s.segments.size(), 3);
  // c: explicit controls
  EXPECT_EQ(s.segments[0].kind, PathSegment::Kind::cubic);
  EXPECT_DOUBLE_EQ(s.segments[0].c1[0], 1);
  EXPECT_DOUBLE_EQ(s.segments[0].c2[0], 2);
  EXPECT_DOUBLE_EQ(s.segments[0].end[0], 3);
  // v: first control is the current point (3,3)
  EXPECT_DOUBLE_EQ(s.segments[1].c1[0], 3);
  EXPECT_DOUBLE_EQ(s.segments[1].c1[1], 3);
  EXPECT_DOUBLE_EQ(s.segments[1].end[0], 5);
  // y: second control coincides with the endpoint (7,7)
  EXPECT_DOUBLE_EQ(s.segments[2].c2[0], 7);
  EXPECT_DOUBLE_EQ(s.segments[2].end[0], 7);
}

// `n` discards the path (its only use today is after a clip operator).
TEST(PdfPageExtractor, path_end_no_paint_emits_nothing) {
  const auto page = run_page("0 0 10 10 re n");
  EXPECT_TRUE(page.empty());
}

// Device colors are snapshotted onto the element as the current fill/stroke.
TEST(PdfPageExtractor, path_snapshots_device_colors) {
  const auto page = run_page("1 0 0 rg 0 0 1 RG 0 0 10 10 re B");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_EQ(p.fill_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[0], 1);
  EXPECT_EQ(p.stroke_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.stroke_color.rgb[2], 1);
}

// A path stroked without `w`/`M`/`J`/`j` carries the PDF initial line params
// (line width 1, miter limit 10, butt cap, miter join), not zeros.
TEST(PdfPageExtractor, path_stroke_uses_initial_line_defaults) {
  const auto page = run_page("0 0 m 10 0 l S");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_DOUBLE_EQ(p.line_width, 1);
  EXPECT_DOUBLE_EQ(p.miter_limit, 10);
  EXPECT_EQ(p.line_cap, 0);
  EXPECT_EQ(p.line_join, 0);
}

// Explicit line-style operators are snapshotted onto the element.
TEST(PdfPageExtractor, path_snapshots_line_params) {
  const auto page = run_page("3 w 1 J 2 j 5 M 0 0 m 10 0 l S");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_DOUBLE_EQ(p.line_width, 3);
  EXPECT_EQ(p.line_cap, 1);
  EXPECT_EQ(p.line_join, 2);
  EXPECT_DOUBLE_EQ(p.miter_limit, 5);
}

// The paint-time CTM is captured so the renderer can scale the (unscaled) line
// width even though the geometry is already flattened to user space.
TEST(PdfPageExtractor, path_captures_paint_ctm) {
  const auto page = run_page("2 0 0 3 5 7 cm 0 0 m 10 0 l S");
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = path_at(page, 0);
  EXPECT_DOUBLE_EQ(p.ctm.a, 2);
  EXPECT_DOUBLE_EQ(p.ctm.d, 3);
  EXPECT_DOUBLE_EQ(p.ctm.e, 5);
  EXPECT_DOUBLE_EQ(p.ctm.f, 7);
}

// Text and paths come out interleaved in paint order.
TEST(PdfPageExtractor, page_preserves_paint_order) {
  const auto page =
      run_page("0 0 10 10 re f BT /F1 10 Tf (Hi) Tj ET 5 5 m 6 6 l S");
  ASSERT_EQ(page.size(), 3);
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[0]));
  EXPECT_TRUE(std::holds_alternative<TextElement>(page[1]));
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[2]));
}
