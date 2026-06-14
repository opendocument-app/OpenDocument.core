#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/pdf/pdf_cmap_parser.hpp>

#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

CMap parse(const std::string &cmap) {
  std::istringstream in(cmap);
  CMapParser parser(in);
  return parser.parse_cmap();
}

} // namespace

TEST(PdfCMap, bfchar_single_byte) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<41> <0041>\n"
                    "<42> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x41\x42"), "AB");
}

TEST(PdfCMap, bfchar_two_byte_codes) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "1 beginbfchar\n"
                    "<0041> <0041>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string(std::string("\x00\x41", 2)), "A");
}

TEST(PdfCMap, bfrange_increment_form) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<41> <43> <0041>\n"
                    "endbfrange\n");

  EXPECT_EQ(cmap.translate_string("\x41\x42\x43"), "ABC");
}

TEST(PdfCMap, bfrange_array_form) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<41> <43> [<0058> <0059> <005A>]\n"
                    "endbfrange\n");

  EXPECT_EQ(cmap.translate_string("\x41\x42\x43"), "XYZ");
}

TEST(PdfCMap, bfchar_multi_character_target) {
  // 0x01 -> "fi" ligature (U+0066 U+0069).
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "1 beginbfchar\n"
                    "<01> <00660069>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x01"), "fi");
}

TEST(PdfCMap, unmapped_code_falls_back_to_identity) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n");

  // No bfchar/bfrange: each two-byte code maps to its own value.
  EXPECT_EQ(cmap.translate_string(std::string("\x00\x41", 2)), "A");
}

TEST(PdfCMap, mixed_code_widths) {
  // Single-byte codes start in 0x00..0x80; two-byte codes start in 0x81..0xFF.
  CMap cmap = parse("2 begincodespacerange\n"
                    "<00> <80>\n"
                    "<8100> <FFFF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<01> <0041>\n"
                    "<8141> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string(std::string("\x01\x81\x41", 3)), "AB");
}
