#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/pdf/pdf_cmap_parser.hpp>

#include <sstream>
#include <string>
#include <string_view>

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
                    "% 99 beginbfchar ignored comment\n"
                    "2 beginbfchar% adjacent comment\n"
                    "<41> % comment between operands\n<0041>\n"
                    "<42> <0042>\n"
                    "endbfchar\nendcmap\n3 ><0S");

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

TEST(PdfCMap, bfrange_matches_the_code_width) {
  CMap cmap = parse("2 begincodespacerange\n"
                    "<00> <7F>\n"
                    "<8000> <FFFF>\n"
                    "endcodespacerange\n"
                    "2 beginbfrange\n"
                    "<41> <42> <0061>\n"
                    "<8041> <8042> <0041>\n"
                    "endbfrange\n");

  EXPECT_EQ(cmap.translate_string(std::string("\x42\x80\x42", 3)), "bB");
}

TEST(PdfCMap, bfchar_wins_over_bfrange) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<0000> <00FF> <0000>\n"
                    "endbfrange\n"
                    "1 beginbfchar\n"
                    "<0042> <0058>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string(std::string("\x00\x41\x00\x42", 4)), "AX");
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

TEST(PdfCMap, reversed_bfrange_is_skipped) {
  // `<43> <41>` is reversed; the range must be ignored rather than wrapping the
  // code counter through ~4 billion iterations.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<43> <41> <0041>\n"
                    "endbfrange\n");

  // Nothing was mapped, so codes fall back to identity.
  EXPECT_EQ(cmap.translate_string("\x41"), "A");
}

TEST(PdfCMap, bfrange_maximum_span_is_mapped) {
  // 256 codes (span 0xFF) is the largest conforming range (only the last byte
  // of the code varies, §9.10.3); it must still be expanded.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<0000> <00FF> <0041>\n"
                    "endbfrange\n");

  // Code 0x0000 maps to U+0041; identity would be the null character instead.
  EXPECT_EQ(cmap.translate_string(std::string("\x00\x00", 2)), "A");
}

TEST(PdfCMap, oversized_bfrange_is_skipped) {
  // 257 codes (span 0x100) exceeds the cap; the range is skipped rather than
  // materializing an unbounded number of entries.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "1 beginbfrange\n"
                    "<0000> <0100> <0041>\n"
                    "endbfrange\n");

  // Code 0x0041 lies inside the skipped range, so it falls back to identity
  // ('A') rather than the value the range would have assigned.
  EXPECT_EQ(cmap.translate_string(std::string("\x00\x41", 2)), "A");
}

TEST(PdfCMap, oversized_code_is_skipped) {
  // A five-byte code cannot be represented; the entry is skipped while a valid
  // sibling entry still maps.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<0102030405> <0041>\n"
                    "<42> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x42"), "B");
}

TEST(PdfCMap, odd_length_destination_is_skipped) {
  // A destination that is not a whole number of UTF-16 units is skipped.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<00> <FF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<41> <00>\n"
                    "<42> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x42"), "B");
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

TEST(PdfCMap, codespace_is_authoritative_without_usecmap) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n");

  EXPECT_FALSE(cmap.inherits_external_cmap());
  EXPECT_TRUE(cmap.has_codespace());
  EXPECT_EQ(cmap.code_width(std::string_view("\0\0", 2)), 2);
}

TEST(PdfCMap, usecmap_disables_local_codespace_authority) {
  // A stream that inherits `Identity-H` (2-byte codespace) via `usecmap` and
  // only declares a 1-byte override codespace must not be trusted to split
  // codes, or the inherited 2-byte codes would be mis-split as single bytes.
  CMap cmap = parse("/Identity-H usecmap\n"
                    "1 begincodespacerange\n"
                    "<20> <20>\n"
                    "endcodespacerange\n"
                    "1 begincidchar\n"
                    "<20> 1\n"
                    "endcidchar\n");

  EXPECT_TRUE(parse("/Identity-H usecmap").inherits_external_cmap());
  EXPECT_TRUE(cmap.inherits_external_cmap());
  // The partial local codespace is no longer authoritative.
  EXPECT_FALSE(cmap.has_codespace());
  // The local CID override is still applied.
  EXPECT_TRUE(cmap.has_cid_map());
  EXPECT_EQ(cmap.cid_for_code(std::string_view("\x20", 1)), 1u);
}

TEST(PdfCMap, imposed_code_width_overrides_codespace) {
  // A simple font's `ToUnicode` CMap carrying the two-byte `<0000> <FFFF>`
  // boilerplate over one-byte entries; splitting by it pairs the codes up.
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<41> <0041>\n"
                    "<42> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x41\x42"), "\xe4\x85\x82"); // U+4142
  EXPECT_EQ(cmap.translate_string("\x41\x42", true), "AB");
}

TEST(PdfCMap, imposed_code_width_falls_back_to_a_padded_entry) {
  CMap cmap = parse("1 begincodespacerange\n"
                    "<0000> <FFFF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<0041> <0041>\n"
                    "<0042> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x41\x42", true), "AB");
}

TEST(PdfCMap, imposed_code_width_keeps_a_mixed_codespace_distinct) {
  // Padding is only for an imposed width; a declared mixed codespace keeps
  // `<20>` and `<2120>` apart.
  CMap cmap = parse("2 begincodespacerange\n"
                    "<00> <20>\n"
                    "<2100> <FFFF>\n"
                    "endcodespacerange\n"
                    "2 beginbfchar\n"
                    "<20> <0041>\n"
                    "<2120> <0042>\n"
                    "endbfchar\n");

  EXPECT_EQ(cmap.translate_string("\x20"), "A");
  EXPECT_EQ(cmap.translate_string("\x21\x20"), "B");
}

TEST(PdfCMap, an_invalid_record_count_skips_its_block) {
  for (const std::string count : {"-1", "4294967296", "1.5", "1 pop", ""}) {
    SCOPED_TRACE(count);
    const CMap cmap = parse(count + " beginbfchar <41> <0058> endbfchar\n"
                                    "1 beginbfchar <42> <0059> endbfchar");
    // the skipped code falls back to itself
    EXPECT_EQ(cmap.translate_string("\x41\x42"), "AY");
  }
}

TEST(PdfCMap, validates_cid_ranges_and_later_overrides) {
  const CMap cmap = parse("5 begincidchar "
                          "<01> -1 <02> 65536 <03> 4294967296 "
                          "<04> 0 <05> 65535 endcidchar "
                          "5 begincidrange "
                          "<10> <11> -1 <20> <21> 65535 "
                          "<30> <31> 4294967296 <40> <42> 100 "
                          "<41> <41> 65535 endcidrange");
  for (const std::string code :
       {"\x01", "\x02", "\x03", "\x10", "\x20", "\x30"}) {
    EXPECT_FALSE(cmap.cid_for_code(code).has_value());
  }
  EXPECT_EQ(cmap.cid_for_code("\x04"), 0u);
  EXPECT_EQ(cmap.cid_for_code("\x05"), 65535u);
  EXPECT_EQ(cmap.cid_for_code("\x40"), 100u);
  EXPECT_EQ(cmap.cid_for_code("\x41"), 65535u);
  EXPECT_EQ(cmap.cid_for_code("\x42"), 102u);
}

TEST(PdfCMap, invalid_codespace_widths) {
  CMap cmap;
  EXPECT_THROW(cmap.add_codespace_range("", ""), std::invalid_argument);
  EXPECT_THROW(cmap.add_codespace_range("a", "ab"), std::invalid_argument);
  EXPECT_THROW(cmap.add_codespace_range("abcde", "abcde"),
               std::invalid_argument);
}

TEST(PdfCMap, codespace_ranges_compare_as_numbers) {
  const CMap cmap =
      parse("1 begincodespacerange <00e0> <028c> endcodespacerange");
  EXPECT_EQ(cmap.code_width("\x02\x49"), 2);
  EXPECT_EQ(cmap.code_width("\x02\x8d"), 1);
}
