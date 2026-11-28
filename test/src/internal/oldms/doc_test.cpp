#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>

#include <limits>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/text/doc_document.hpp>
#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_style.hpp>

#include <internal/oldms/oldms_test_util.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace odr;
using odr::test::oldms::append_u16;
using odr::test::oldms::append_u32;
using odr::test::oldms::collect_text;

// A compressed (1-byte-per-CP) piece is "an array of 8-bit Unicode characters"
// ([MS-DOC] 2.9.73 / 2.4.1 step 6): each byte is a code point, except the
// 0x82-0x9F values that map to the Windows-1252 punctuation block. The decoder
// must UTF-8-encode the code point, not emit the raw byte (which is invalid
// UTF-8 for 0xA0-0xFF).
TEST(OldMs, doc_read_string_compressed) {
  using internal::oldms::text::read_string_compressed;

  // ASCII: byte == code point == single UTF-8 byte.
  {
    std::istringstream in(std::string("Hi!"));
    EXPECT_EQ(read_string_compressed(in, 3), "Hi!");
  }

  // Mapped byte: 0x92 -> U+2019 (RIGHT SINGLE QUOTATION MARK) -> UTF-8 E2
  // 80 99.
  {
    std::istringstream in(std::string("\x92", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xE2\x80\x99");
  }

  // High Latin-1 byte not in the table: 0xE9 -> U+00E9 ('é') -> UTF-8 C3 A9.
  // This is the case the old `push_back` got wrong (it emitted the lone 0xE9).
  {
    std::istringstream in(std::string("\xE9", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xC3\xA9");
  }

  // Mixed run: "café" stored as A-S 'c''a''f' + 0xE9.
  {
    std::istringstream in(std::string("caf\xE9", 4));
    EXPECT_EQ(read_string_compressed(in, 4), "caf\xC3\xA9");
  }
}

namespace {

std::string doc_prl(const std::uint16_t opcode, const std::string &operand) {
  std::string prl;
  append_u16(prl, opcode);
  prl += operand;
  return prl;
}

/// A minimal FIB ([MS-DOC] 2.5.1): nFib97, table stream /1Table, with the
/// FcLcbs for clx, plcfBteChpx and sttbfFfn, and any `others` by their index
/// in FibRgFcLcb97.
std::string
make_fib(const std::uint32_t ccp_text, const internal::oldms::text::FcLcb clx,
         const internal::oldms::text::FcLcb plcf_bte_chpx,
         const internal::oldms::text::FcLcb sttbf_ffn,
         const std::vector<std::pair<std::size_t, internal::oldms::text::FcLcb>>
             &others = {}) {
  std::string fib;
  // FibBase (32 bytes)
  append_u16(fib, 0xA5EC);      // wIdent
  append_u16(fib, 0x00C1);      // nFib97
  append_u16(fib, 0);           // unused
  append_u16(fib, 0x0409);      // lid
  append_u16(fib, 0);           // pnNext
  append_u16(fib, 0x0200);      // flags: fWhichTblStm = 1
  append_u16(fib, 0x00C1);      // nFibBack
  append_u32(fib, 0);           // lKey
  fib += std::string(2, '\0');  // envr, flags
  fib += std::string(12, '\0'); // reserved3-6
  // csw + fibRgW
  append_u16(fib, 14);
  fib += std::string(28, '\0');
  // cslw + fibRgLw; ccpText is the 4th 32-bit field
  append_u16(fib, 22);
  std::string rg_lw(88, '\0');
  std::string ccp;
  append_u32(ccp, ccp_text);
  rg_lw.replace(12, 4, ccp);
  fib += rg_lw;
  // cbRgFcLcb + FibRgFcLcb97: plcfBteChpx is FcLcb index 12, sttbfFfn 15,
  // clx 33
  append_u16(fib, 93);
  std::string rg(744, '\0');
  const auto put_fclcb = [&rg](const std::size_t index,
                               const internal::oldms::text::FcLcb value) {
    std::string bytes;
    append_u32(bytes, value.fc);
    append_u32(bytes, value.lcb);
    rg.replace(index * 8, 8, bytes);
  };
  put_fclcb(12, plcf_bte_chpx);
  put_fclcb(15, sttbf_ffn);
  put_fclcb(33, clx);
  for (const auto &[index, value] : others) {
    put_fclcb(index, value);
  }
  fib += rg;
  // cswNew
  append_u16(fib, 0);
  return fib;
}

/// An SttbfFfn ([MS-DOC] 2.9.286) with the given font names.
std::string make_sttbf_ffn(const std::vector<std::string> &names) {
  std::string sttb;
  append_u16(sttb, static_cast<std::uint16_t>(names.size())); // cData
  append_u16(sttb, 0);                                        // cbExtra
  for (const std::string &name : names) {
    std::string ffn(39, '\0'); // FfnFixed
    for (const char c : name) {
      ffn.push_back(c);
      ffn.push_back('\0');
    }
    ffn += std::string(2, '\0'); // terminator
    sttb.push_back(static_cast<char>(ffn.size()));
    sttb += ffn;
  }
  return sttb;
}

/// An STD ([MS-DOC] 2.9.258) with a 10-byte StdfBase, the name, and the
/// given UPXs, each sized and padded to even.
std::string make_std(const std::uint16_t sti, const std::uint8_t stk,
                     const std::uint16_t istd_base, const std::string &name,
                     const std::vector<std::string> &upxs) {
  std::string std;
  append_u16(std, sti);
  append_u16(std, static_cast<std::uint16_t>(stk | istd_base << 4));
  append_u16(std, static_cast<std::uint16_t>(upxs.size()));
  append_u16(std, 0); // bchUpe
  append_u16(std, 0); // grfstd
  append_u16(std, static_cast<std::uint16_t>(name.size()));
  for (const char c : name) {
    append_u16(std, static_cast<std::uint8_t>(c));
  }
  append_u16(std, 0);
  for (const std::string &upx : upxs) {
    append_u16(std, static_cast<std::uint16_t>(upx.size()));
    std += upx;
    if (upx.size() % 2 != 0) {
      std.push_back('\0');
    }
  }
  return std;
}

/// An STSH ([MS-DOC] 2.9.271) of `stds`, indexed by istd, with the default
/// font `ftc_ascii`; an empty STD is an unused istd.
std::string make_stsh(const std::vector<std::string> &stds,
                      const std::uint16_t ftc_ascii) {
  std::string stsh;
  append_u16(stsh, 18);                                      // cbStshi
  append_u16(stsh, static_cast<std::uint16_t>(stds.size())); // cstd
  append_u16(stsh, 10);                                      // cbSTDBaseInFile
  stsh += std::string(8, '\0');
  append_u16(stsh, ftc_ascii);
  stsh += std::string(4, '\0');
  for (const std::string &std : stds) {
    append_u16(stsh, static_cast<std::uint16_t>(std.size()));
    stsh += std;
  }
  return stsh;
}

/// An istd and a grpprl, as a UpxPapx or GrpPrlAndIstd ([MS-DOC] 2.9.114).
std::string istd_and(const std::uint16_t istd, const std::string &grpprl) {
  std::string result;
  append_u16(result, istd);
  return result + grpprl;
}

} // namespace

// Character SPRMs ([MS-DOC] 2.6.1) map onto TextStyle; unknown SPRMs are
// skipped via their operand size, malformed grpprls throw.
TEST(OldMs, doc_apply_character_sprms) {
  using internal::oldms::text::apply_character_sprms;

  TextStyle base;
  base.font_size = Measure("10pt");
  const std::vector<std::string> fonts = {"Arial", "Courier New"};

  {
    std::string grpprl;
    grpprl += doc_prl(0x0835, "\x01"); // bold: on
    grpprl += doc_prl(0x0836, "\x81"); // italic: invert style default -> on
    grpprl += doc_prl(0x0837, "\x80"); // strike: match style default -> off
    grpprl += doc_prl(0x2A3E, "\x01"); // single underline
    grpprl += doc_prl(0x4A43, std::string("\x20\x00", 2)); // 32 half-points
    grpprl += doc_prl(0x4A4F, std::string("\x01\x00", 2)); // font index 1
    grpprl += doc_prl(0x6870, std::string("\x11\x22\x33\x00", 4)); // COLORREF
    grpprl += doc_prl(0x2A0C, "\x07"); // highlight: yellow
    const TextStyle style = apply_character_sprms(base, grpprl, fonts);
    EXPECT_EQ(style.font_weight, FontWeight::bold);
    EXPECT_EQ(style.font_style, FontStyle::italic);
    EXPECT_EQ(style.font_line_through, false);
    EXPECT_EQ(style.font_underline, true);
    EXPECT_EQ(style.font_size, Measure("16pt"));
    EXPECT_EQ(style.font_name, "Courier New");
    ASSERT_TRUE(style.font_color.has_value());
    EXPECT_EQ(style.font_color->rgb(), 0x112233u);
    ASSERT_TRUE(style.background_color.has_value());
    EXPECT_EQ(style.background_color->rgb(), 0xFFFF00u);
  }

  {
    // Legacy palette color; unknown fixed-size and variable-size SPRMs are
    // skipped without disturbing later Prls.
    std::string grpprl;
    grpprl += doc_prl(0x2A42, "\x06");                         // ico: red
    grpprl += doc_prl(0x0838, "\x01");                         // unknown, 1 B
    grpprl += doc_prl(0xC875, std::string("\x02\xAA\xBB", 3)); // unknown, var
    grpprl += doc_prl(0x0835, "\x01");                         // bold: on
    const TextStyle style = apply_character_sprms(base, grpprl, fonts);
    ASSERT_TRUE(style.font_color.has_value());
    EXPECT_EQ(style.font_color->rgb(), 0xFF0000u);
    EXPECT_EQ(style.font_weight, FontWeight::bold);
  }

  {
    // cvAuto ([MS-DOC] 2.9.43) resets an earlier explicit color.
    TextStyle colored = base;
    colored.font_color = Color::from_rgb(0x123456);
    const TextStyle style = apply_character_sprms(
        colored, doc_prl(0x6870, std::string("\x00\x00\x00\xFF", 4)), fonts);
    EXPECT_FALSE(style.font_color.has_value());
  }

  {
    // sprmCRgFtc0 with an empty SttbfFfn: 0 is valid ([MS-DOC] 2.6.1) and
    // leaves the font unset; anything else is out of range.
    const std::vector<std::string> no_fonts;
    const TextStyle style = apply_character_sprms(
        base, doc_prl(0x4A4F, std::string("\x00\x00", 2)), no_fonts);
    EXPECT_FALSE(style.font_name.has_value());
    EXPECT_THROW(
        apply_character_sprms(base, doc_prl(0x4A4F, std::string("\x01\x00", 2)),
                              no_fonts),
        std::runtime_error);
  }

  // Out-of-range and negative sprmCRgFtc0 indexes throw.
  EXPECT_THROW(apply_character_sprms(
                   base, doc_prl(0x4A4F, std::string("\x02\x00", 2)), fonts),
               std::runtime_error);
  EXPECT_THROW(apply_character_sprms(
                   base, doc_prl(0x4A4F, std::string("\xFF\xFF", 2)), fonts),
               std::runtime_error);

  // Truncated operand, half a Sprm, and an out-of-range sprmCHps all throw.
  EXPECT_THROW(apply_character_sprms(base, doc_prl(0x4A43, "\x01"), fonts),
               std::runtime_error);
  EXPECT_THROW(apply_character_sprms(base, std::string("\x35", 1), fonts),
               std::runtime_error);
  EXPECT_THROW(apply_character_sprms(
                   base, doc_prl(0x4A43, std::string("\x01\x00", 2)), fonts),
               std::runtime_error);
}

// A ToggleOperand ([MS-DOC] 2.9.327) of 0x80 or 0x81 resolves against the
// style it applies to.
TEST(OldMs, doc_toggle_operands_follow_the_style) {
  using internal::oldms::text::apply_character_sprms;

  TextStyle bold;
  bold.font_weight = FontWeight::bold;
  EXPECT_EQ(
      apply_character_sprms(bold, doc_prl(0x0835, "\x80"), {}).font_weight,
      FontWeight::bold);
  EXPECT_EQ(
      apply_character_sprms(bold, doc_prl(0x0835, "\x81"), {}).font_weight,
      FontWeight::normal);
  EXPECT_EQ(
      apply_character_sprms(bold, doc_prl(0x0835, std::string(1, '\0')), {})
          .font_weight,
      FontWeight::normal);
}

// Paragraph SPRMs ([MS-DOC] 2.6.2) map onto ParagraphStyle in inches.
TEST(OldMs, doc_apply_paragraph_sprms) {
  using internal::oldms::text::apply_paragraph_sprms;
  const auto twips = [](const double value) {
    return Measure(value / 1440.0, DynamicUnit("in"));
  };

  {
    std::string grpprl;
    grpprl += doc_prl(0x2461, "\x03");                             // justify
    grpprl += doc_prl(0x845E, std::string("\xA0\x05", 2));         // left 1440
    grpprl += doc_prl(0x845D, std::string("\x48\xFF", 2));         // right -184
    grpprl += doc_prl(0x8460, std::string("\x10\xFE", 2));         // first -496
    grpprl += doc_prl(0xA413, std::string("\x78\x00", 2));         // before 120
    grpprl += doc_prl(0xA414, std::string("\xC8\x00", 2));         // after 200
    grpprl += doc_prl(0x6412, std::string("\x14\x01\x01\x00", 4)); // 1.15
    grpprl += doc_prl(0x2407, "\x01"); // page break before
    const ParagraphStyle style = apply_paragraph_sprms({}, grpprl);
    EXPECT_EQ(style.text_align, TextAlign::justify);
    EXPECT_EQ(style.margin.left, twips(1440));
    EXPECT_EQ(style.margin.right, twips(-184));
    EXPECT_EQ(style.text_indent, twips(-496));
    EXPECT_EQ(style.margin.top, twips(120));
    EXPECT_EQ(style.margin.bottom, twips(200));
    EXPECT_EQ(style.line_height, Measure(276 / 2.4, DynamicUnit("%")));
    EXPECT_EQ(style.break_before, BreakType::page);
  }

  {
    // The Word 97 variants, an exact line height, and the irregularly sized
    // sprmTDefTable and sprmPChgTabs skipped without disturbing later Prls.
    std::string grpprl;
    grpprl += doc_prl(0x2403, "\x01");                     // center
    grpprl += doc_prl(0x840F, std::string("\x40\x02", 2)); // left 576
    grpprl += doc_prl(0xD608, std::string("\x03\x00\xAA\xBB", 4));
    grpprl += doc_prl(0xC615, std::string("\xFF\x01\x01\x02\x03\x04"
                                          "\x01\x05\x06\x07",
                                          10));
    grpprl += doc_prl(0x6412, std::string("\x10\xFF\x00\x00", 4));
    grpprl += doc_prl(0x2407, std::string(1, '\0'));
    const ParagraphStyle style = apply_paragraph_sprms({}, grpprl);
    EXPECT_EQ(style.text_align, TextAlign::center);
    EXPECT_EQ(style.margin.left, twips(576));
    EXPECT_EQ(style.line_height, twips(240));
    EXPECT_EQ(style.break_before, BreakType::none);
  }

  {
    // An at-least height of 0 is single spacing, clearing the style's.
    ParagraphStyle spaced;
    spaced.line_height = Measure("150%");
    EXPECT_FALSE(
        apply_paragraph_sprms(spaced, doc_prl(0x6412, std::string(4, '\0')))
            .line_height.has_value());
  }

  EXPECT_THROW(apply_paragraph_sprms({}, doc_prl(0xA413, "\x01")),
               std::runtime_error);
  EXPECT_THROW(
      apply_paragraph_sprms({}, doc_prl(0xD608, std::string("\x00\x00", 2))),
      std::runtime_error);
}

// Section SPRMs ([MS-DOC] 2.6.4) map onto PageLayout over the defaults.
TEST(OldMs, doc_apply_section_sprms) {
  using internal::oldms::text::apply_section_sprms;
  using internal::oldms::text::default_page_layout;
  const auto twips = [](const double value) {
    return Measure(value / 1440.0, DynamicUnit("in"));
  };

  const PageLayout defaults = default_page_layout();
  EXPECT_EQ(defaults.width, twips(12240));
  EXPECT_EQ(defaults.height, twips(15840));
  EXPECT_EQ(defaults.margin.left, twips(1800));
  EXPECT_EQ(defaults.margin.top, twips(1440));

  std::string grpprl;
  grpprl += doc_prl(0xB01F, std::string("\x82\x2E", 2)); // 11906
  grpprl += doc_prl(0xB020, std::string("\xC6\x41", 2)); // 16838
  grpprl += doc_prl(0xB021, std::string("\xA0\x05", 2)); // left 1440
  grpprl += doc_prl(0x9023, std::string("\xC8\xFB", 2)); // top -1080
  grpprl += doc_prl(0x301D, "\x02");                     // landscape
  const PageLayout layout = apply_section_sprms(defaults, grpprl);
  EXPECT_EQ(layout.width, twips(11906));
  EXPECT_EQ(layout.height, twips(16838));
  EXPECT_EQ(layout.margin.left, twips(1440));
  EXPECT_EQ(layout.margin.right, twips(1800));
  EXPECT_EQ(layout.margin.top, twips(1080));
  EXPECT_EQ(layout.margin.bottom, twips(1440));
  EXPECT_EQ(layout.print_orientation, PrintOrientation::landscape);
}

// Paragraph styles resolve through their istdBase chain; other kinds of style
// and unused istds fall back to Normal.
TEST(OldMs, doc_read_style_sheet) {
  namespace word = internal::oldms::text;
  const std::vector<std::string> fonts = {"Arial", "Times New Roman"};

  const std::string stsh = make_stsh(
      {make_std(0, 1, 0x0FFF, "Normal",
                {istd_and(0, doc_prl(0xA413, std::string("\x78\x00", 2))),
                 doc_prl(0x4A43, std::string("\x18\x00", 2))}),
       make_std(
           1, 1, 0, "Heading",
           {istd_and(1, doc_prl(0x2403, "\x01")), doc_prl(0x0835, "\x81")}),
       make_std(65, 2, 0x0FFF, "Strong", {doc_prl(0x0835, "\x01")}), ""},
      1);
  std::istringstream in("xx" + stsh);
  const word::StyleSheet sheet = word::read_style_sheet(
      in, {2, static_cast<std::uint32_t>(stsh.size())}, fonts);

  const word::StyleSheet::Entry &normal = sheet.paragraph_style(0);
  EXPECT_EQ(normal.text.font_size, Measure("12pt"));
  EXPECT_EQ(normal.text.font_name, "Times New Roman");
  EXPECT_FALSE(normal.text.font_weight.has_value());
  EXPECT_EQ(normal.paragraph.margin.top,
            Measure(120 / 1440.0, DynamicUnit("in")));

  const word::StyleSheet::Entry &heading = sheet.paragraph_style(1);
  EXPECT_EQ(heading.paragraph.text_align, TextAlign::center);
  EXPECT_EQ(heading.paragraph.margin.top, normal.paragraph.margin.top);
  EXPECT_EQ(heading.text.font_weight, FontWeight::bold);
  EXPECT_EQ(heading.text.font_size, Measure("12pt"));

  for (const std::uint16_t istd : {2, 3, 99}) {
    EXPECT_EQ(sheet.paragraph_style(istd).text.font_size, Measure("12pt"));
    EXPECT_FALSE(sheet.paragraph_style(istd).text.font_weight.has_value());
  }

  // Without a style sheet: the bare defaults.
  std::istringstream empty;
  EXPECT_EQ(word::read_style_sheet(empty, {}, fonts)
                .paragraph_style(0)
                .text.font_size,
            Measure("10pt"));

  // A style based on itself is malformed.
  const std::string cyclic =
      make_stsh({make_std(0, 1, 0, "Normal", {istd_and(0, ""), ""})}, 0);
  std::istringstream cyclic_in(cyclic);
  EXPECT_THROW(
      word::read_style_sheet(
          cyclic_in, {0, static_cast<std::uint32_t>(cyclic.size())}, fonts),
      std::runtime_error);
}

TEST(OldMs, doc_read_font_names) {
  const std::string sttb = make_sttbf_ffn({"Arial", "Times New Roman"});
  std::istringstream in(sttb);
  const std::vector<std::string> names = internal::oldms::text::read_font_names(
      in, {0, static_cast<std::uint32_t>(sttb.size())});
  ASSERT_EQ(names.size(), 2);
  EXPECT_EQ(names[0], "Arial");
  EXPECT_EQ(names[1], "Times New Roman");
  in.clear();
  EXPECT_THROW(internal::oldms::text::read_font_names(in, {0, 4}),
               std::runtime_error);
  // a name without its NUL is kept whole rather than refusing the document
  std::string unterminated = make_sttbf_ffn({"Arial"});
  unterminated[unterminated.size() - 2] = 'x';
  std::istringstream bad(unterminated);
  EXPECT_EQ(internal::oldms::text::read_font_names(
                bad, {0, static_cast<std::uint32_t>(unterminated.size())}),
            std::vector<std::string>{"Arialx"});
}

// End-to-end over a synthetic .doc: the PlcBteChpx/ChpxFkp run boundaries
// split the text into styled spans ([MS-DOC] 2.4.6.2), the default run keeps
// the 10pt default, and the paragraph carries a style for empty-height.
TEST(OldMs, doc_character_formatting) {
  const std::string body = "plain bold\r";
  constexpr std::uint32_t text_fc = 1536;

  // ChpxFkp page ([MS-DOC] 2.9.33) at pn 2: run "plain " -> no Chpx (rgb 0),
  // run "bold\r" -> bold + font 0.
  std::string fkp(512, '\0');
  {
    std::string head;
    append_u32(head, text_fc);
    append_u32(head, text_fc + 6);
    append_u32(head, text_fc + 11);
    head.push_back('\0');                   // rgb[0]: default properties
    head.push_back(static_cast<char>(240)); // rgb[1]: Chpx at byte 480
    fkp.replace(0, head.size(), head);

    std::string chpx;
    chpx += doc_prl(0x0835, "\x01");                     // bold
    chpx += doc_prl(0x4A4F, std::string("\x00\x00", 2)); // font index 0
    fkp[480] = static_cast<char>(chpx.size());
    fkp.replace(481, chpx.size(), chpx);
    fkp[511] = 2; // crun
  }

  // Table stream: Clx at 0 (one compressed piece), SttbfFfn at 64,
  // PlcBteChpx at 128.
  std::string plc_pcd;
  append_u32(plc_pcd, 0);
  append_u32(plc_pcd, 11);
  append_u16(plc_pcd, 0);                           // Pcd flags
  append_u32(plc_pcd, (1u << 30) | (text_fc * 2u)); // fCompressed | fc
  append_u16(plc_pcd, 0);                           // prm
  std::string clx;
  clx.push_back('\x02');
  append_u32(clx, static_cast<std::uint32_t>(plc_pcd.size()));
  clx += plc_pcd;

  const std::string sttbf_ffn = make_sttbf_ffn({"Arial"});

  std::string plc_bte;
  append_u32(plc_bte, text_fc);
  append_u32(plc_bte, text_fc + 11);
  append_u32(plc_bte, 2); // PnFkpChpx: page at 2 * 512

  std::string table(256, '\0');
  table.replace(0, clx.size(), clx);
  table.replace(64, sttbf_ffn.size(), sttbf_ffn);
  table.replace(128, plc_bte.size(), plc_bte);

  std::string word_document =
      make_fib(11, {0, static_cast<std::uint32_t>(clx.size())},
               {128, static_cast<std::uint32_t>(plc_bte.size())},
               {64, static_cast<std::uint32_t>(sttbf_ffn.size())});
  word_document.resize(1024, '\0');
  word_document += fkp;
  word_document += body;

  auto files = std::make_shared<internal::VirtualFilesystem>();
  files->copy(std::make_shared<internal::MemoryFile>(word_document),
              internal::AbsPath("/WordDocument"));
  files->copy(std::make_shared<internal::MemoryFile>(table),
              internal::AbsPath("/1Table"));
  const Document document(
      std::make_shared<internal::oldms::text::Document>(files));

  std::vector<Element> paragraphs;
  for (const Element child : document.root_element().children()) {
    paragraphs.push_back(child);
  }
  ASSERT_EQ(paragraphs.size(), 1);
  ASSERT_EQ(paragraphs[0].type(), ElementType::paragraph);

  std::vector<Element> spans;
  for (const Element child : paragraphs[0].children()) {
    ASSERT_EQ(child.type(), ElementType::span);
    spans.push_back(child);
  }
  ASSERT_EQ(spans.size(), 2);

  EXPECT_EQ(collect_text(spans[0]), "plain ");
  const TextStyle plain = spans[0].as_span().style();
  EXPECT_EQ(plain.font_size, Measure("10pt"));
  EXPECT_FALSE(plain.font_weight.has_value());
  EXPECT_FALSE(plain.font_name.has_value());

  EXPECT_EQ(collect_text(spans[1]), "bold");
  const TextStyle bold = spans[1].as_span().style();
  EXPECT_EQ(bold.font_weight, FontWeight::bold);
  EXPECT_EQ(bold.font_name, "Arial");
  EXPECT_EQ(bold.font_size, Measure("10pt"));

  EXPECT_EQ(paragraphs[0].as_paragraph().text_style().font_size,
            Measure("10pt"));

  // The same physical bytes cannot satisfy a shorter declared Clx or a longer
  // body.
  for (const bool short_clx : {true, false}) {
    std::string invalid = word_document;
    const std::string fib =
        make_fib(short_clx ? 11 : 12,
                 {0, short_clx ? 5u : static_cast<std::uint32_t>(clx.size())},
                 {128, static_cast<std::uint32_t>(plc_bte.size())},
                 {64, static_cast<std::uint32_t>(sttbf_ffn.size())});
    invalid.replace(0, fib.size(), fib);
    auto invalid_files = std::make_shared<internal::VirtualFilesystem>(*files);
    ASSERT_TRUE(invalid_files->remove(internal::AbsPath("/WordDocument")));
    ASSERT_TRUE(
        invalid_files->copy(std::make_shared<internal::MemoryFile>(invalid),
                            internal::AbsPath("/WordDocument")));
    EXPECT_THROW(internal::oldms::text::Document{invalid_files},
                 std::runtime_error);
  }
}

// A paragraph takes its PAPX from its mark and lies on its style; its runs lie
// on the same style; the first section gives the page.
TEST(OldMs, doc_paragraph_formatting) {
  const std::string body = "Head\rBody text\r";
  constexpr std::uint32_t text_fc = 1536;
  constexpr std::uint32_t sepx_fc = 1600;

  // PapxFkp ([MS-DOC] 2.9.174) at pn 2: "Head\r" is Heading with 10pt after,
  // its PapxInFkp sized in words; "Body text\r" is Normal, right aligned,
  // sized 2 * cb - 1.
  std::string fkp(512, '\0');
  {
    std::string head;
    append_u32(head, text_fc);
    append_u32(head, text_fc + 5);
    append_u32(head, text_fc + 15);
    fkp.replace(0, head.size(), head);
    fkp[head.size()] = static_cast<char>(200);
    fkp[head.size() + 13] = static_cast<char>(220);

    const std::string heading =
        istd_and(1, doc_prl(0xA414, std::string("\xC8\x00", 2)));
    fkp[400] = '\0';
    fkp[401] = static_cast<char>(heading.size() / 2);
    fkp.replace(402, heading.size(), heading);

    const std::string normal = istd_and(0, doc_prl(0x2461, "\x02"));
    fkp[440] = static_cast<char>((normal.size() + 1) / 2);
    fkp.replace(441, normal.size(), normal);
    fkp[511] = 2; // cpara
  }

  std::string sepx;
  {
    std::string grpprl;
    grpprl += doc_prl(0xB01F, std::string("\x82\x2E", 2)); // A4 width
    grpprl += doc_prl(0x9023, std::string("\xC8\xFB", 2)); // top -1080
    append_u16(sepx, static_cast<std::uint16_t>(grpprl.size()));
    sepx += grpprl;
  }

  // Table stream: Clx at 0, SttbfFfn at 64, PlcBtePapx at 192, STSH at 256,
  // PlcfSed at 512.
  std::string plc_pcd;
  append_u32(plc_pcd, 0);
  append_u32(plc_pcd, static_cast<std::uint32_t>(body.size()));
  append_u16(plc_pcd, 0);
  append_u32(plc_pcd, (1u << 30) | (text_fc * 2u));
  append_u16(plc_pcd, 0);
  std::string clx;
  clx.push_back('\x02');
  append_u32(clx, static_cast<std::uint32_t>(plc_pcd.size()));
  clx += plc_pcd;

  const std::string sttbf_ffn = make_sttbf_ffn({"Arial", "Times New Roman"});

  std::string plc_bte_papx;
  append_u32(plc_bte_papx, text_fc);
  append_u32(plc_bte_papx, text_fc + 15);
  append_u32(plc_bte_papx, 2);

  const std::string stsh = make_stsh(
      {make_std(0, 1, 0x0FFF, "Normal",
                {istd_and(0, doc_prl(0xA413, std::string("\x78\x00", 2))),
                 doc_prl(0x4A43, std::string("\x18\x00", 2))}),
       make_std(
           1, 1, 0, "Heading",
           {istd_and(1, doc_prl(0x2403, "\x01")), doc_prl(0x0835, "\x81")})},
      1);

  std::string plcf_sed;
  append_u32(plcf_sed, 0);
  append_u32(plcf_sed, static_cast<std::uint32_t>(body.size()));
  append_u16(plcf_sed, 0); // fn
  append_u32(plcf_sed, sepx_fc);
  append_u16(plcf_sed, 0); // fnMpr
  append_u32(plcf_sed, 0); // fcMpr

  std::string table(640, '\0');
  table.replace(0, clx.size(), clx);
  table.replace(64, sttbf_ffn.size(), sttbf_ffn);
  table.replace(192, plc_bte_papx.size(), plc_bte_papx);
  table.replace(256, stsh.size(), stsh);
  table.replace(512, plcf_sed.size(), plcf_sed);

  std::string word_document =
      make_fib(static_cast<std::uint32_t>(body.size()),
               {0, static_cast<std::uint32_t>(clx.size())}, {},
               {64, static_cast<std::uint32_t>(sttbf_ffn.size())},
               {{1, {256, static_cast<std::uint32_t>(stsh.size())}},
                {6, {512, static_cast<std::uint32_t>(plcf_sed.size())}},
                {13, {192, static_cast<std::uint32_t>(plc_bte_papx.size())}}});
  word_document.resize(1024, '\0');
  word_document += fkp;
  word_document += body;
  word_document.resize(sepx_fc, '\0');
  word_document += sepx;

  auto files = std::make_shared<internal::VirtualFilesystem>();
  files->copy(std::make_shared<internal::MemoryFile>(word_document),
              internal::AbsPath("/WordDocument"));
  files->copy(std::make_shared<internal::MemoryFile>(table),
              internal::AbsPath("/1Table"));
  const Document document(
      std::make_shared<internal::oldms::text::Document>(files));
  const auto twips = [](const double value) {
    return Measure(value / 1440.0, DynamicUnit("in"));
  };

  const PageLayout layout =
      document.root_element().as_text_root().page_layout();
  EXPECT_EQ(layout.width, twips(11906));
  EXPECT_EQ(layout.height, twips(15840));
  EXPECT_EQ(layout.margin.top, twips(1080));
  EXPECT_EQ(layout.margin.left, twips(1800));

  std::vector<Element> paragraphs;
  for (const Element child : document.root_element().children()) {
    paragraphs.push_back(child);
  }
  ASSERT_EQ(paragraphs.size(), 2);

  const ParagraphStyle heading = paragraphs[0].as_paragraph().style();
  EXPECT_EQ(heading.text_align, TextAlign::center);
  EXPECT_EQ(heading.margin.top, twips(120));
  EXPECT_EQ(heading.margin.bottom, twips(200));
  const TextStyle heading_text =
      (*paragraphs[0].children().begin()).as_span().style();
  EXPECT_EQ(heading_text.font_weight, FontWeight::bold);
  EXPECT_EQ(heading_text.font_size, Measure("12pt"));
  EXPECT_EQ(heading_text.font_name, "Times New Roman");

  const ParagraphStyle normal = paragraphs[1].as_paragraph().style();
  EXPECT_EQ(normal.text_align, TextAlign::right);
  EXPECT_EQ(normal.margin.top, twips(120));
  EXPECT_FALSE(normal.margin.bottom.has_value());
  const TextStyle normal_text =
      (*paragraphs[1].children().begin()).as_span().style();
  EXPECT_FALSE(normal_text.font_weight.has_value());
  EXPECT_EQ(normal_text.font_size, Measure("12pt"));
  EXPECT_EQ(collect_text(paragraphs[1]), "Body text");
}

TEST(OldMs, doc_fib_versions_respect_counted_arrays) {
  namespace word = internal::oldms::text;
  const auto parse = [](const std::string &bytes) {
    std::istringstream in(bytes + "NEXT");
    word::ParsedFib fib{};
    word::read(in, fib);
    EXPECT_EQ(in.peek(), 'N');
    EXPECT_EQ(fib.fibRgFcLcb.clx.fc, 123);
    EXPECT_EQ(fib.fibRgFcLcb.clx.lcb, 456);
    return fib;
  };
  const std::string base = make_fib(7, {123, 456}, {}, {});
  EXPECT_EQ(parse(base).ccpText(), 7);
  std::string extended = base;
  extended[152] =
      95; // cbRgFcLcb: two unused FcLcb entries after the common prefix.
  extended.insert(extended.size() - 2, 16, '\0');
  EXPECT_EQ(parse(extended).ccpText(), 7);
  for (const std::uint16_t version :
       {word::nFib2000, word::nFib2002, word::nFib2003, word::nFib2007}) {
    std::string bytes = base.substr(0, base.size() - 2);
    const std::uint16_t words = version == word::nFib2007 ? 5 : 2;
    append_u16(bytes, words);
    append_u16(bytes, version);
    bytes.append((words - 1) * 2, '\0');
    EXPECT_EQ(parse(bytes).nFibNew, version);
  }
  std::string future = base;
  future[2] = '\x13';
  future[3] = '\x01';
  EXPECT_FALSE(parse(future).nFibNew.has_value());

  std::string undersized = base.substr(0, base.size() - 2);
  append_u16(undersized, 1);
  append_u16(undersized, word::nFib2007);
  undersized.append(8,
                    '\0'); // Bytes after the declared array cannot satisfy it.
  EXPECT_THROW(parse(undersized), std::runtime_error);
  std::string invalid = base;
  invalid[0] = 0;
  EXPECT_THROW(parse(invalid), std::runtime_error);
  EXPECT_THROW(parse(make_fib(0x80000000u, {}, {}, {})), std::runtime_error);
}

TEST(OldMs, doc_plc_reads_unaligned_bytes_and_checks_indices) {
  namespace word = internal::oldms::text;
  std::string bytes("x");
  append_u32(bytes, 0);
  append_u32(bytes, 3);
  append_u16(bytes, 0);
  append_u32(bytes, (1u << 30) | 200u);
  append_u16(bytes, 0);
  const word::PlcPcdMap map(bytes.data() + 1, bytes.size() - 1);
  EXPECT_EQ(map.n(), 1);
  EXPECT_EQ(map.aCP(1), 3);
  EXPECT_EQ(map.aData(0).fc.fc, 200);
  EXPECT_THROW(static_cast<void>(map.aCP(2)), std::out_of_range);
  EXPECT_THROW(static_cast<void>(map.aData(1)), std::out_of_range);
  EXPECT_THROW(static_cast<void>(word::PlcPcdMap(bytes.data(), 3).n()),
               std::runtime_error);
}

TEST(OldMs, doc_character_pieces_validate_ranges) {
  namespace word = internal::oldms::text;
  word::CharacterIndex index;
  index.append(0, 3, 100, true);
  index.append(3, 2, 200, false);
  EXPECT_EQ((*index.find(2)).data_offset, 100);
  EXPECT_EQ((*index.find(3)).data_offset, 200);
  EXPECT_EQ(index.find(5), index.end());
  const word::CharacterIndex other;
  EXPECT_NE(index.begin(), other.begin());
  EXPECT_THROW(index.append(6, 1, 300, true), std::runtime_error);
  EXPECT_THROW(index.append(4, 2, 300, true), std::runtime_error);
  EXPECT_THROW(
      index.append(5, std::numeric_limits<std::size_t>::max(), 0, true),
      std::runtime_error);
  EXPECT_THROW(
      index.append(5, 2, std::numeric_limits<std::uint32_t>::max() - 1, false),
      std::runtime_error);
  EXPECT_EQ(index.size(), 2);
}
