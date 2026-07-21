#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/text/doc_document.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_style.hpp>

#include <internal/oldms/oldms_test_util.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
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

void append_doc_u16(std::string &out, const std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>(value >> 8));
}

void append_doc_u32(std::string &out, const std::uint32_t value) {
  append_doc_u16(out, static_cast<std::uint16_t>(value & 0xFFFF));
  append_doc_u16(out, static_cast<std::uint16_t>(value >> 16));
}

std::string doc_prl(const std::uint16_t opcode, const std::string &operand) {
  std::string prl;
  append_doc_u16(prl, opcode);
  prl += operand;
  return prl;
}

/// A minimal FIB ([MS-DOC] 2.5.1): nFib97, table stream /1Table, with the
/// three FcLcbs the parser reads (clx, plcfBteChpx, sttbfFfn).
std::string make_fib(const std::uint32_t ccp_text,
                     const internal::oldms::text::FcLcb clx,
                     const internal::oldms::text::FcLcb plcf_bte_chpx,
                     const internal::oldms::text::FcLcb sttbf_ffn) {
  std::string fib;
  // FibBase (32 bytes)
  append_doc_u16(fib, 0xA5EC);  // wIdent
  append_doc_u16(fib, 0x00C1);  // nFib97
  append_doc_u16(fib, 0);       // unused
  append_doc_u16(fib, 0x0409);  // lid
  append_doc_u16(fib, 0);       // pnNext
  append_doc_u16(fib, 0x0200);  // flags: fWhichTblStm = 1
  append_doc_u16(fib, 0x00C1);  // nFibBack
  append_doc_u32(fib, 0);       // lKey
  fib += std::string(2, '\0');  // envr, flags
  fib += std::string(12, '\0'); // reserved3-6
  // csw + fibRgW
  append_doc_u16(fib, 14);
  fib += std::string(28, '\0');
  // cslw + fibRgLw; ccpText is the 4th 32-bit field
  append_doc_u16(fib, 22);
  std::string rg_lw(88, '\0');
  std::string ccp;
  append_doc_u32(ccp, ccp_text);
  rg_lw.replace(12, 4, ccp);
  fib += rg_lw;
  // cbRgFcLcb + FibRgFcLcb97: plcfBteChpx is FcLcb index 12, sttbfFfn 15,
  // clx 33
  append_doc_u16(fib, 93);
  std::string rg(744, '\0');
  const auto put_fclcb = [&rg](const std::size_t index,
                               const internal::oldms::text::FcLcb value) {
    std::string bytes;
    append_doc_u32(bytes, value.fc);
    append_doc_u32(bytes, value.lcb);
    rg.replace(index * 8, 8, bytes);
  };
  put_fclcb(12, plcf_bte_chpx);
  put_fclcb(15, sttbf_ffn);
  put_fclcb(33, clx);
  fib += rg;
  // cswNew
  append_doc_u16(fib, 0);
  return fib;
}

/// An SttbfFfn ([MS-DOC] 2.9.286) with the given font names.
std::string make_sttbf_ffn(const std::vector<std::string> &names) {
  std::string sttb;
  append_doc_u16(sttb, static_cast<std::uint16_t>(names.size())); // cData
  append_doc_u16(sttb, 0);                                        // cbExtra
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

} // namespace

// Character SPRMs ([MS-DOC] 2.6.1) map onto TextStyle; unknown SPRMs are
// skipped via their operand size, malformed grpprls throw.
TEST(OldMs, doc_apply_character_sprms) {
  using internal::oldms::text::apply_character_sprms;

  TextStyle base;
  base.font_size = Measure("10pt");
  const std::vector<const char *> fonts = {"Arial", "Courier New"};

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
    EXPECT_STREQ(style.font_name, "Courier New");
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
    colored.font_color = Color(std::uint32_t{0x123456});
    const TextStyle style = apply_character_sprms(
        colored, doc_prl(0x6870, std::string("\x00\x00\x00\xFF", 4)), fonts);
    EXPECT_FALSE(style.font_color.has_value());
  }

  {
    // sprmCRgFtc0 with an empty SttbfFfn: 0 is valid ([MS-DOC] 2.6.1) and
    // leaves the font unset; anything else is out of range.
    const std::vector<const char *> no_fonts;
    const TextStyle style = apply_character_sprms(
        base, doc_prl(0x4A4F, std::string("\x00\x00", 2)), no_fonts);
    EXPECT_EQ(style.font_name, nullptr);
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

TEST(OldMs, doc_read_font_names) {
  const std::string sttb = make_sttbf_ffn({"Arial", "Times New Roman"});
  std::istringstream in(sttb);
  const std::vector<std::string> names = internal::oldms::text::read_font_names(
      in, {0, static_cast<std::uint32_t>(sttb.size())});
  ASSERT_EQ(names.size(), 2);
  EXPECT_EQ(names[0], "Arial");
  EXPECT_EQ(names[1], "Times New Roman");
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
    append_doc_u32(head, text_fc);
    append_doc_u32(head, text_fc + 6);
    append_doc_u32(head, text_fc + 11);
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
  append_doc_u32(plc_pcd, 0);
  append_doc_u32(plc_pcd, 11);
  append_doc_u16(plc_pcd, 0);                           // Pcd flags
  append_doc_u32(plc_pcd, (1u << 30) | (text_fc * 2u)); // fCompressed | fc
  append_doc_u16(plc_pcd, 0);                           // prm
  std::string clx;
  clx.push_back('\x02');
  append_doc_u32(clx, static_cast<std::uint32_t>(plc_pcd.size()));
  clx += plc_pcd;

  const std::string sttbf_ffn = make_sttbf_ffn({"Arial"});

  std::string plc_bte;
  append_doc_u32(plc_bte, text_fc);
  append_doc_u32(plc_bte, text_fc + 11);
  append_doc_u32(plc_bte, 2); // PnFkpChpx: page at 2 * 512

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
  EXPECT_EQ(plain.font_name, nullptr);

  EXPECT_EQ(collect_text(spans[1]), "bold");
  const TextStyle bold = spans[1].as_span().style();
  EXPECT_EQ(bold.font_weight, FontWeight::bold);
  EXPECT_STREQ(bold.font_name, "Arial");
  EXPECT_EQ(bold.font_size, Measure("10pt"));

  EXPECT_EQ(paragraphs[0].as_paragraph().text_style().font_size,
            Measure("10pt"));
}
