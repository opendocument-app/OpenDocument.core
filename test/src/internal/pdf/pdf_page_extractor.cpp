#include <odr/internal/pdf/pdf_page_extractor.hpp>

#include <odr/logger.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_color.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>

#include <initializer_list>
#include <memory>
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

// Image XObjects with no browser-ready bytes (an unhandled codec) are skipped:
// `Do` is a no-op.
TEST(PdfPageExtractor, image_xobject_ignored) {
  XObject image;
  image.subtype = XObject::Subtype::image;
  Resources res;
  res.x_object["Im0"] = &image;

  EXPECT_TRUE(extract_page("/Im0 Do", res, Logger::null()).empty());
}

// A stencil image mask (`/ImageMask true`) is painted in the current fill
// colour at `Do` time, producing a recoloured RGBA PNG `ImageElement`.
TEST(PdfPageExtractor, stencil_image_xobject_recoloured) {
  XObject mask;
  mask.subtype = XObject::Subtype::image;
  mask.stencil_mask = true;
  mask.stencil_width = 2;
  mask.stencil_height = 1;
  mask.stencil_samples = std::string(1, '\x40'); // 1 bpc, bits 0,1 (padded)
  Resources res;
  res.x_object["Im0"] = &mask;

  const auto elements = extract_page("1 0 0 rg /Im0 Do", res, Logger::null());
  ASSERT_EQ(elements.size(), 1);
  const auto *image = std::get_if<ImageElement>(&elements[0]);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->mime, "image/png");
  EXPECT_FALSE(image->data.empty());
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
  EXPECT_EQ(texts[0].rendering_mode, TextRenderingMode::invisible);
}

// A composite font with no `/ToUnicode` and no usable predefined encoding has
// no recoverable Unicode: the segment is marked `no_unicode` with empty text.
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

// A Type3 font: code `first_char` maps (via `/Differences`) to glyph `name`,
// drawn by `char_proc` in glyph space, with `font_matrix` (glyph -> text space)
// and a single glyph-space `width`.
Font type3_font(int first_char, const std::string &name, double width,
                std::string char_proc, const Transform2D &font_matrix) {
  Font font;
  font.first_char = first_char;
  font.widths = {width};
  Encoding encoding(BaseEncoding::standard);
  encoding.set_difference(static_cast<std::uint8_t>(first_char), name);
  font.encoding = encoding;
  Type3Data type3;
  type3.font_matrix = font_matrix;
  type3.char_procs[name] = std::move(char_proc);
  font.type3 = std::move(type3);
  return font;
}

} // namespace

// A Type3 glyph runs its char proc through the page machinery at the glyph
// transform (`/FontMatrix` x size x `Tm` x CTM): the glyph paints as ordinary
// path elements, and the shown run stays selectable but paints no visible text
// of its own (`render_as_graphics`).
TEST(PdfPageExtractor, type3_glyph_paints_char_proc) {
  // The char proc fills a 1000x1000 glyph-space box; FontMatrix 0.001 maps it
  // to a 1x1 em box, so at size 10 placed at (100, 700) it spans +10 user
  // units.
  Font font = type3_font('A', "a", 1000, "0 0 1000 1000 re f",
                         Transform2D::scaling(0.001, 0.001));
  Resources res;
  res.font["F1"] = &font;

  const auto page =
      extract_page("BT /F1 10 Tf 100 700 Td (A) Tj ET", res, Logger::null());

  // The selectable text element (emitted first), then the char-proc fill.
  ASSERT_EQ(page.size(), 2);
  const auto &text = std::get<TextElement>(page.at(0));
  EXPECT_TRUE(text.render_as_graphics);
  EXPECT_EQ(text.codes, "A");
  EXPECT_EQ(text.text, "a"); // 'A' -> glyph "a" -> U+0061 via the AGL

  const PathElement &glyph = std::get<PathElement>(page.at(1));
  EXPECT_TRUE(glyph.fill);
  ASSERT_EQ(glyph.subpaths.size(), 1);
  const Subpath &s = glyph.subpaths[0];
  EXPECT_NEAR(s.start[0], 100, 1e-9); // glyph origin at the text origin
  EXPECT_NEAR(s.start[1], 700, 1e-9);
  ASSERT_FALSE(s.segments.empty());
  EXPECT_NEAR(s.segments[0].end[0], 110, 1e-9); // 100 + 1000 * 0.001 * 10
  EXPECT_NEAR(s.segments[0].end[1], 700, 1e-9);
}

// A Type3 glyph advance comes from `/Widths` scaled by `/FontMatrix` (glyph
// space), not the fixed 1/1000 em of other fonts.
TEST(PdfPageExtractor, type3_advance_uses_font_matrix) {
  Font font = type3_font('A', "a", 500, "0 0 1000 1000 re f",
                         Transform2D::scaling(0.001, 0.001));
  Resources res;
  res.font["F1"] = &font;

  // Two glyphs; the second is placed by the first's advance (500 * 0.001 = 0.5
  // em, * size 10 = 5 user units).
  const auto page =
      extract_page("BT /F1 10 Tf 0 0 Td (AA) Tj ET", res, Logger::null());

  // text, glyph, glyph (the two char procs).
  ASSERT_EQ(page.size(), 3);
  const PathElement &second = std::get<PathElement>(page.at(2));
  ASSERT_EQ(second.subpaths.size(), 1);
  EXPECT_NEAR(second.subpaths[0].start[0], 5, 1e-9); // advanced by 0.5 em
}

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

// Text and paths come out interleaved in paint order.
TEST(PdfPageExtractor, page_preserves_paint_order) {
  const auto page =
      run_page("0 0 10 10 re f BT /F1 10 Tf (Hi) Tj ET 5 5 m 6 6 l S");
  ASSERT_EQ(page.size(), 3);
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[0]));
  EXPECT_TRUE(std::holds_alternative<TextElement>(page[1]));
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[2]));
}

// --- stroke parameters ----------------------------------------

// Line width and cap/join/miter snapshot onto the stroked element; the default
// line width is 1 (ISO 32000-1 8.4.1).
TEST(PdfPageExtractor, stroke_line_params) {
  const auto def = run_page("0 0 m 10 0 l S");
  EXPECT_DOUBLE_EQ(path_at(def, 0).line_width, 1);

  const auto page = run_page("4 w 2 J 1 j 6 M 0 0 m 10 0 l S");
  const PathElement &p = path_at(page, 0);
  EXPECT_DOUBLE_EQ(p.line_width, 4);
  EXPECT_EQ(p.line_cap, 2);
  EXPECT_EQ(p.line_join, 1);
  EXPECT_DOUBLE_EQ(p.miter_limit, 6);
}

// The dash array and phase parse onto the element.
TEST(PdfPageExtractor, stroke_dash_pattern) {
  const auto page = run_page("[3 2] 1 d 0 0 m 10 0 l S");
  const PathElement &p = path_at(page, 0);
  ASSERT_EQ(p.dash_array.size(), 2);
  EXPECT_DOUBLE_EQ(p.dash_array[0], 3);
  EXPECT_DOUBLE_EQ(p.dash_array[1], 2);
  EXPECT_DOUBLE_EQ(p.dash_phase, 1);
}

// A scaling CTM folds into the stroke width, the dash lengths and the phase, so
// they share the geometry's user space.
TEST(PdfPageExtractor, stroke_width_and_dash_scale_with_ctm) {
  const auto page = run_page("3 0 0 3 0 0 cm 4 w [3 2] 1 d 0 0 m 10 0 l S");
  const PathElement &p = path_at(page, 0);
  EXPECT_DOUBLE_EQ(p.line_width, 12); // 4 * 3
  ASSERT_EQ(p.dash_array.size(), 2);
  EXPECT_DOUBLE_EQ(p.dash_array[0], 9); // 3 * 3
  EXPECT_DOUBLE_EQ(p.dash_array[1], 6); // 2 * 3
  EXPECT_DOUBLE_EQ(p.dash_phase, 3);    // 1 * 3
}

// An empty dash array clears a previous one (solid line).
TEST(PdfPageExtractor, stroke_dash_reset_to_solid) {
  const auto page = run_page("[3 2] 0 d [] 0 d 0 0 m 10 0 l S");
  EXPECT_TRUE(path_at(page, 0).dash_array.empty());
}

// --- clipping --------------------------------------------------

// A `W n` clip rect limits a later fill: the fill carries the clip region (the
// rect), nonzero rule, while the clip itself paints nothing.
TEST(PdfPageExtractor, clip_rect_limits_later_fill) {
  const auto page = run_page("0 0 100 100 re W n 10 10 200 200 re f");
  ASSERT_EQ(page.size(), 1); // only the fill emits an element
  const PathElement &p = path_at(page, 0);
  EXPECT_TRUE(p.fill);
  ASSERT_EQ(p.clip.size(), 1);
  EXPECT_FALSE(p.clip[0].even_odd);
  ASSERT_EQ(p.clip[0].subpaths.size(), 1);
  EXPECT_DOUBLE_EQ(p.clip[0].subpaths[0].start[0], 0);
  EXPECT_DOUBLE_EQ(p.clip[0].subpaths[0].segments[0].end[0], 100); // 0 + 100
}

// `W*` selects the even-odd clip rule.
TEST(PdfPageExtractor, clip_evenodd_rule) {
  const auto page = run_page("0 0 10 10 re W* n 0 0 5 5 re f");
  ASSERT_EQ(page.size(), 1);
  ASSERT_EQ(path_at(page, 0).clip.size(), 1);
  EXPECT_TRUE(path_at(page, 0).clip[0].even_odd);
}

// The painting operator that installs a clip is itself clipped only by the
// *previous* clip, not the one it establishes (ISO 32000-1 8.5.4).
TEST(PdfPageExtractor, clip_excludes_its_own_paint) {
  // `re W f`: the fill paints under no clip; the rect becomes a clip
  // afterwards.
  const auto page = run_page("0 0 10 10 re W f 0 0 5 5 re f");
  ASSERT_EQ(page.size(), 2);
  EXPECT_TRUE(path_at(page, 0).clip.empty()); // the clip-establishing fill
  ASSERT_EQ(path_at(page, 1).clip.size(), 1); // the next fill is clipped
}

// Nested clips intersect: a second `W n` adds a region, so a later fill carries
// both, in order.
TEST(PdfPageExtractor, clip_nested_intersect) {
  const auto page =
      run_page("0 0 100 100 re W n 10 10 50 50 re W n 20 20 10 10 re f");
  ASSERT_EQ(page.size(), 1);
  ASSERT_EQ(path_at(page, 0).clip.size(), 2);
}

// The clip is part of the saved/restored graphics state: a clip set inside
// `q`/`Q` does not leak to content after the `Q`.
TEST(PdfPageExtractor, clip_save_restore) {
  const auto page = run_page("q 0 0 10 10 re W n 0 0 5 5 re f Q 0 0 5 5 re f");
  ASSERT_EQ(page.size(), 2);
  EXPECT_EQ(path_at(page, 0).clip.size(), 1); // inside q/Q: clipped
  EXPECT_TRUE(path_at(page, 1).clip.empty()); // after Q: clip gone
}

// A form XObject's `/BBox` clips its content (ISO 32000-1 8.10.2), mapped
// through the form `/Matrix` + CTM; the clip is scoped to the form.
TEST(PdfPageExtractor, form_bbox_clips_content) {
  XObject form = form_x_object("0 0 100 100 re f");
  form.bbox = std::array<double, 4>{10, 20, 30, 40};
  Resources res;
  res.x_object["Fm0"] = &form;

  const auto page = extract_page("/Fm0 Do", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  ASSERT_EQ(p.clip.size(), 1);
  const Subpath &s = p.clip[0].subpaths[0];
  EXPECT_DOUBLE_EQ(s.start[0], 10);
  EXPECT_DOUBLE_EQ(s.start[1], 20);
  EXPECT_DOUBLE_EQ(s.segments[1].end[0], 30); // x1
  EXPECT_DOUBLE_EQ(s.segments[1].end[1], 40); // y1
}

// --- colour spaces ---------------------------------------------

namespace {

std::shared_ptr<ColorSpaceDef> rgb_space() {
  auto def = std::make_shared<ColorSpaceDef>();
  def->kind = ColorSpaceKind::device_rgb;
  def->components = 3;
  return def;
}

} // namespace

// `cs`/`scn` over a named resource colour space resolve the fill colour to RGB.
TEST(PdfPageExtractor, scn_resolves_named_color_space) {
  Resources res;
  res.color_space["CS0"] = rgb_space();

  const auto page = extract_page("/CS0 cs 0.2 0.4 0.6 scn 0 0 10 10 re f", res,
                                 Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  EXPECT_EQ(p.fill_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[0], 0.2);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[1], 0.4);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[2], 0.6);
}

// A Separation space samples its tint transform to RGB at `scn` time.
TEST(PdfPageExtractor, scn_separation_through_tint) {
  // tint: type 2, C0 = white, C1 = red -> tint(t) = (1, 1-t, 1-t).
  Dictionary tint;
  tint["FunctionType"] = Object(Integer{2});
  tint["Domain"] = [] {
    std::vector<Object> h{Object(Real{0}), Object(Real{1})};
    return Object(Array(std::move(h)));
  }();
  tint["C0"] = [] {
    std::vector<Object> h{Object(Real{1}), Object(Real{1}), Object(Real{1})};
    return Object(Array(std::move(h)));
  }();
  tint["C1"] = [] {
    std::vector<Object> h{Object(Real{1}), Object(Real{0}), Object(Real{0})};
    return Object(Array(std::move(h)));
  }();
  tint["N"] = Object(Real{1});
  std::vector<Object> array{Object(Name{"Separation"}), Object(Name{"Spot"}),
                            Object(Name{"DeviceRGB"}), Object(tint)};
  ColorSpaceContext ctx;
  ctx.resolve = [](const Object &o) { return o; };
  ctx.load_stream = [](const Object &) { return std::string{}; };
  ctx.named = nullptr;

  Resources res;
  res.color_space["Sep"] =
      parse_color_space(Object(Array(std::move(array))), ctx);

  const auto page =
      extract_page("/Sep cs 1.0 scn 0 0 10 10 re f", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  EXPECT_EQ(p.fill_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[0], 1.0); // full tint -> red
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[1], 0.0);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[2], 0.0);
}

// A device colour operator (`rg`) clears a previously set resource colour
// space, so a following device colour is not mis-resolved through it.
TEST(PdfPageExtractor, device_color_clears_color_space) {
  Resources res;
  res.color_space["CS0"] = rgb_space();

  const auto page = extract_page("/CS0 cs 0.1 0.2 0.3 scn 1 0 0 rg "
                                 "0 0 10 10 re f",
                                 res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  EXPECT_EQ(p.fill_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[0], 1.0);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[1], 0.0);
}

// --- shadings & shading patterns -------------------------------

namespace {

// A minimal axial shading with two stops (black to white).
std::shared_ptr<Shading> axial_shading() {
  auto shading = std::make_shared<Shading>();
  shading->type = 2;
  shading->coords = {0, 0, 1, 0, 0, 0};
  shading->stops = {GradientStop{0.0, {0, 0, 0}}, GradientStop{1.0, {1, 1, 1}}};
  return shading;
}

} // namespace

// `scn` naming a `/PatternType 2` pattern fills the path through the pattern's
// shading; `fill_shading` is resolved and the pattern `/Matrix` carried.
TEST(PdfPageExtractor, scn_shading_pattern_fills_path) {
  Pattern pattern;
  pattern.type = Pattern::Type::shading;
  pattern.shading = axial_shading();
  pattern.matrix = Transform2D::translation(5, 7);
  Resources res;
  res.pattern["P0"] = &pattern;

  const auto page =
      extract_page("/Pattern cs /P0 scn 0 0 10 10 re f", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  ASSERT_NE(p.fill_shading, nullptr);
  EXPECT_EQ(p.fill_shading->type, 2);
  EXPECT_TRUE(p.fill);
  EXPECT_DOUBLE_EQ(p.shading_transform.e, 5);
  EXPECT_DOUBLE_EQ(p.shading_transform.f, 7);
}

// `scn` naming an unknown pattern leaves the fill with no shading (a plain
// colour fill); the path still paints.
TEST(PdfPageExtractor, scn_unknown_pattern_has_no_shading) {
  Resources res;
  const auto page = extract_page("/Pattern cs /Missing scn 0 0 10 10 re f", res,
                                 Logger::null());
  ASSERT_EQ(page.size(), 1);
  EXPECT_EQ(std::get<PathElement>(page[0]).fill_shading, nullptr);
}

// `scn` naming a `/PatternType 1` tiling pattern fills the path with the
// pattern; `fill_pattern` is resolved and the pattern `/Matrix` carried.
TEST(PdfPageExtractor, scn_tiling_pattern_fills_path) {
  Pattern pattern;
  pattern.type = Pattern::Type::tiling;
  pattern.matrix = Transform2D::translation(3, 4);
  Resources res;
  res.pattern["P1"] = &pattern;

  const auto page =
      extract_page("/Pattern cs /P1 scn 0 0 10 10 re f", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  ASSERT_NE(p.fill_pattern, nullptr);
  EXPECT_EQ(p.fill_pattern->type, Pattern::Type::tiling);
  EXPECT_EQ(p.fill_shading, nullptr);
  EXPECT_TRUE(p.fill);
  EXPECT_DOUBLE_EQ(p.pattern_transform.e, 3);
  EXPECT_DOUBLE_EQ(p.pattern_transform.f, 4);
}

// An uncoloured tiling pattern (`/PaintType 2`) records the current fill colour
// alongside the pattern, so the renderer can paint the cell in it.
TEST(PdfPageExtractor, scn_uncoloured_tiling_pattern_carries_colour) {
  Pattern pattern;
  pattern.type = Pattern::Type::tiling;
  pattern.paint_type = 2;
  Resources res;
  res.pattern["P2"] = &pattern;

  // The colour precedes the pattern selection in the Pattern colour space.
  const auto page = extract_page("/Pattern cs 1 0 0 /P2 scn 0 0 10 10 re f",
                                 res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  ASSERT_NE(p.fill_pattern, nullptr);
  EXPECT_EQ(p.fill_pattern->paint_type, 2);
}

// An uncoloured pattern selected through a *named* Pattern colour space with an
// underlying base (`[/Pattern /DeviceRGB]`) resolves its leading components
// through that base — `1 0 0` is red, not black (the base would be ignored if
// the Pattern space's `to_rgb` dropped it).
TEST(PdfPageExtractor, scn_uncoloured_tiling_pattern_colour_through_base) {
  Pattern pattern;
  pattern.type = Pattern::Type::tiling;
  pattern.paint_type = 2;

  std::vector<Object> array{Object(Name{"Pattern"}), Object(Name{"DeviceRGB"})};
  ColorSpaceContext ctx;
  ctx.resolve = [](const Object &o) { return o; };
  ctx.load_stream = [](const Object &) { return std::string{}; };
  ctx.named = nullptr;

  Resources res;
  res.color_space["CS1"] =
      parse_color_space(Object(Array(std::move(array))), ctx);
  res.pattern["P2"] = &pattern;

  const auto page =
      extract_page("/CS1 cs 1 0 0 /P2 scn 0 0 10 10 re f", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const PathElement &p = std::get<PathElement>(page[0]);
  ASSERT_NE(p.fill_pattern, nullptr);
  EXPECT_EQ(p.fill_color.space, ColorSpace::device_rgb);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[0], 1.0);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[1], 0.0);
  EXPECT_DOUBLE_EQ(p.fill_color.rgb[2], 0.0);
}

// The `sh` operator floods the current clip with a named `/Shading`, emitting a
// `ShadingElement` placed by the CTM.
TEST(PdfPageExtractor, sh_emits_shading_element) {
  Resources res;
  res.shading["Sh0"] = axial_shading();

  const auto page =
      extract_page("q 2 0 0 2 10 20 cm /Sh0 sh Q", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const ShadingElement &s = std::get<ShadingElement>(page[0]);
  ASSERT_NE(s.shading, nullptr);
  EXPECT_EQ(s.shading->type, 2);
  EXPECT_DOUBLE_EQ(s.transform.a, 2);
  EXPECT_DOUBLE_EQ(s.transform.e, 10);
  EXPECT_DOUBLE_EQ(s.transform.f, 20);
}

// `sh` naming an unknown shading emits nothing.
TEST(PdfPageExtractor, sh_unknown_shading_emits_nothing) {
  Resources res;
  EXPECT_TRUE(extract_page("/Missing sh", res, Logger::null()).empty());
}

// --- image XObjects (JPEG pass-through) ------------------------

namespace {

XObject jpeg_x_object(std::string data) {
  XObject x_object;
  x_object.subtype = XObject::Subtype::image;
  x_object.image_data = std::move(data);
  x_object.image_mime = "image/jpeg";
  return x_object;
}

} // namespace

// `Do` on a pass-through image XObject emits an `ImageElement` placed by the
// CTM, carrying the encoded bytes verbatim.
TEST(PdfPageExtractor, image_xobject_emitted_at_ctm) {
  XObject image = jpeg_x_object("JFIF-bytes");
  Resources res;
  res.x_object["Im0"] = &image;

  const auto page =
      extract_page("q 2 0 0 3 10 20 cm /Im0 Do Q", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const ImageElement &img = std::get<ImageElement>(page[0]);
  EXPECT_EQ(img.data, "JFIF-bytes");
  EXPECT_EQ(img.mime, "image/jpeg");
  EXPECT_DOUBLE_EQ(img.transform.a, 2); // unit square -> 2 wide
  EXPECT_DOUBLE_EQ(img.transform.d, 3); // 3 tall
  EXPECT_DOUBLE_EQ(img.transform.e, 10);
  EXPECT_DOUBLE_EQ(img.transform.f, 20);
}

// An image whose codec is not a pass-through (no `image_data`) is skipped, as
// is an unknown XObject — `Do` emits nothing.
TEST(PdfPageExtractor, image_xobject_without_data_skipped) {
  XObject image; // subtype image, but no decoded pass-through bytes
  image.subtype = XObject::Subtype::image;
  Resources res;
  res.x_object["Im0"] = &image;

  EXPECT_TRUE(extract_page("/Im0 Do", res, Logger::null()).empty());
}

// An image is clipped by the current clip, like a path.
TEST(PdfPageExtractor, image_xobject_carries_clip) {
  XObject image = jpeg_x_object("bytes");
  Resources res;
  res.x_object["Im0"] = &image;

  const auto page =
      extract_page("0 0 50 50 re W n /Im0 Do", res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  EXPECT_EQ(std::get<ImageElement>(page[0]).clip.size(), 1);
}

// Images interleave with paths and text in paint order.
TEST(PdfPageExtractor, image_in_paint_order) {
  XObject image = jpeg_x_object("bytes");
  Resources res;
  res.x_object["Im0"] = &image;

  const auto page =
      extract_page("0 0 10 10 re f /Im0 Do 5 5 m 6 6 l S", res, Logger::null());
  ASSERT_EQ(page.size(), 3);
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[0]));
  EXPECT_TRUE(std::holds_alternative<ImageElement>(page[1]));
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[2]));
}

// --- inline images (BI/ID/EI) ---------------------------------

namespace {

const std::string png_signature = {
    static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n',
    static_cast<char>(0x1A), '\n'};

std::string raw_bytes(std::initializer_list<int> values) {
  std::string result;
  for (const int v : values) {
    result.push_back(static_cast<char>(v));
  }
  return result;
}

} // namespace

// `BI … ID <bytes> EI` tokenizes as one operator and emits an `ImageElement`
// placed by the CTM, the raster re-encoded as PNG.
TEST(PdfPageExtractor, inline_image_emitted_at_ctm) {
  const std::string content = "q 2 0 0 3 10 20 cm "
                              "BI /W 2 /H 1 /CS /RGB /BPC 8 ID " +
                              raw_bytes({10, 20, 30, 40, 50, 60}) + "\nEI Q";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const ImageElement &img = std::get<ImageElement>(page[0]);
  EXPECT_EQ(img.mime, "image/png");
  EXPECT_EQ(img.data.substr(0, 8), png_signature);
  EXPECT_DOUBLE_EQ(img.transform.a, 2);
  EXPECT_DOUBLE_EQ(img.transform.d, 3);
  EXPECT_DOUBLE_EQ(img.transform.e, 10);
  EXPECT_DOUBLE_EQ(img.transform.f, 20);
}

// The binary payload (which may itself contain `EI`) does not corrupt the
// parse: content after the inline image is still tokenized.
TEST(PdfPageExtractor, inline_image_then_path) {
  const std::string content = "BI /W 1 /H 1 /CS /G /BPC 8 ID " +
                              raw_bytes({128}) + "\nEI 0 0 10 10 re f";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 2);
  EXPECT_TRUE(std::holds_alternative<ImageElement>(page[0]));
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[1]));
}

// An unfiltered image's length is fixed by its geometry, so raw sample bytes
// that happen to spell `E I <white-space>` (here the first pixel, 0x45 0x49
// 0x20) are not mistaken for the `EI` terminator: the image survives intact and
// the following operator is still parsed.
TEST(PdfPageExtractor, inline_image_data_containing_ei) {
  const std::string content = "BI /W 2 /H 1 /CS /RGB /BPC 8 ID " +
                              raw_bytes({0x45, 0x49, 0x20, 10, 20, 30}) +
                              "\nEI 0 0 10 10 re f";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 2);
  EXPECT_TRUE(std::holds_alternative<ImageElement>(page[0]));
  EXPECT_TRUE(std::holds_alternative<PathElement>(page[1]));
}

// A Flate (`/F /Fl`) inline image is decoded and re-encoded as PNG.
TEST(PdfPageExtractor, inline_image_flate) {
  const std::string samples = raw_bytes({10, 20, 30, 40, 50, 60});
  const std::string content =
      "BI /W 2 /H 1 /CS /RGB /BPC 8 /F /Fl ID " +
      odr::internal::crypto::util::zlib_deflate(samples) + "\nEI";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const ImageElement &img = std::get<ImageElement>(page[0]);
  EXPECT_EQ(img.mime, "image/png");
  EXPECT_EQ(img.data.substr(0, 8), png_signature);
}

// A Flate inline image with a PNG predictor: the white-space separating the
// data from `EI` must not leak into the encoded payload. A stray trailing byte
// extends the inflated stream by one, so it is no longer a whole number of
// predictor rows and decoding throws.
TEST(PdfPageExtractor, inline_image_flate_predictor_drops_separator) {
  // One PNG predictor row: a leading filter-type tag (0, None) then the
  // samples.
  const std::string row = raw_bytes({0, 10, 20, 30, 40, 50, 60});
  const std::string content =
      "BI /W 2 /H 1 /CS /RGB /BPC 8 /F /Fl "
      "/DP <</Predictor 15 /Colors 3 /Columns 2 /BitsPerComponent 8>> ID " +
      odr::internal::crypto::util::zlib_deflate(row) + "\nEI";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const ImageElement &img = std::get<ImageElement>(page[0]);
  EXPECT_EQ(img.mime, "image/png");
  EXPECT_EQ(img.data.substr(0, 8), png_signature);
}

// `/CS` naming a page `/ColorSpace` resource is resolved through it.
TEST(PdfPageExtractor, inline_image_named_resource_color_space) {
  auto color_space = std::make_shared<ColorSpaceDef>();
  color_space->kind = ColorSpaceKind::device_gray;
  color_space->components = 1;
  Resources res;
  res.color_space["Gray"] = color_space;

  const std::string content =
      "BI /W 1 /H 1 /CS /Gray /BPC 8 ID " + raw_bytes({200}) + "\nEI";
  const auto page = extract_page(content, res, Logger::null());
  ASSERT_EQ(page.size(), 1);
  EXPECT_EQ(std::get<ImageElement>(page[0]).mime, "image/png");
}

// An inline image mask (`/IM true`) is a 1-bpc stencil painted in the current
// fill colour, emitted as a recoloured RGBA PNG `ImageElement`.
TEST(PdfPageExtractor, inline_image_mask_recoloured) {
  const std::string content =
      "0 0 1 rg BI /W 8 /H 1 /IM true /BPC 1 ID " + raw_bytes({0xAA}) + "\nEI";
  const auto page = extract_page(content, Resources{}, Logger::null());
  ASSERT_EQ(page.size(), 1);
  const auto *image = std::get_if<ImageElement>(&page[0]);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->mime, "image/png");
  EXPECT_FALSE(image->data.empty());
}
