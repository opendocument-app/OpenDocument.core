#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/spreadsheet/xls_document.hpp>
#include <odr/internal/oldms/spreadsheet/xls_io.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>

#include <bit>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;

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

// Collects the text content of an element subtree, joining a slide's paragraphs
// with newlines so the paragraph structure is observable.
std::string collect_text(const Element element) {
  if (element.type() == ElementType::text) {
    return element.as_text().content();
  }
  std::string result;
  bool first = true;
  for (const Element child : element.children()) {
    if (!first && child.type() == ElementType::paragraph) {
      result += '\n';
    }
    result += collect_text(child);
    first = false;
  }
  return result;
}

} // namespace

namespace {

void append_u16(std::string &out, const std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>(value >> 8));
}

void append_record(std::string &out, const std::uint16_t type,
                   const std::string &body) {
  append_u16(out, type);
  append_u16(out, static_cast<std::uint16_t>(body.size()));
  out += body;
}

} // namespace

// An SST string ([MS-XLS] 2.5.293) can be split mid-string by a CONTINUE
// record, whose first byte is a fresh flags byte that may switch the encoding
// for the remainder.
TEST(OldMs, xls_string_split_across_continue) {
  using internal::oldms::spreadsheet::BiffReader;

  std::string body;
  append_u16(body, 4);    // cch
  body.push_back('\x00'); // flags: compressed
  body += "ab";           // first half, 1 byte per character

  std::string continuation;
  continuation.push_back('\x01'); // fresh flags: now UTF-16
  continuation += std::string("c\0d\0", 4);

  std::string stream;
  append_record(stream, 0x00FC /* SST */, body);
  append_record(stream, 0x003C /* CONTINUE */, continuation);

  std::istringstream in(stream);
  BiffReader reader(in);
  ASSERT_TRUE(reader.next_record());
  EXPECT_EQ(reader.read_xl_unicode_rich_extended_string(), "abcd");
}

// Formatting runs (fRichSt) are skipped; the skip can cross a CONTINUE
// boundary (without a flags byte — only character data gets one), and the
// next string starts right after.
TEST(OldMs, xls_rich_string_runs_across_continue) {
  using internal::oldms::spreadsheet::BiffReader;

  std::string body;
  append_u16(body, 2);                // cch
  body.push_back('\x08');             // flags: fRichSt
  append_u16(body, 2);                // cRun: 2 FormatRuns = 8 bytes
  body += "hi";                       // characters
  body += std::string("\0\0\0\0", 4); // first FormatRun

  std::string continuation;
  continuation += std::string("\0\0\0\0", 4); // second FormatRun
  append_u16(continuation, 1);                // next string: cch
  continuation.push_back('\x00');             // flags: compressed
  continuation += "x";

  std::string stream;
  append_record(stream, 0x00FC /* SST */, body);
  append_record(stream, 0x003C /* CONTINUE */, continuation);

  std::istringstream in(stream);
  BiffReader reader(in);
  ASSERT_TRUE(reader.next_record());
  EXPECT_EQ(reader.read_xl_unicode_rich_extended_string(), "hi");
  EXPECT_EQ(reader.read_xl_unicode_rich_extended_string(), "x");
}

// RkNumber ([MS-XLS] 2.5.217): bit 0 = fX100, bit 1 = fInt, the rest is a
// 30-bit signed integer or the high 30 bits of an IEEE double. Building the
// inputs from raw on-disk encodings also pins the bit-field layout.
TEST(OldMs, xls_decode_rk) {
  using internal::oldms::spreadsheet::format_number;
  using internal::oldms::spreadsheet::RkNumber;

  const auto rk = [](const std::uint32_t raw) {
    return std::bit_cast<RkNumber>(raw);
  };

  EXPECT_EQ(rk((1234u << 2) | 0x2).decode(), 1234.0);
  EXPECT_EQ(rk((static_cast<std::uint32_t>(-56) << 2) | 0x2).decode(), -56.0);
  EXPECT_EQ(rk((12345u << 2) | 0x2 | 0x1).decode(), 123.45);
  // 0.5 = 0x3FE0000000000000; its high 30 bits survive RK encoding.
  EXPECT_EQ(rk(0x3FE00000).decode(), 0.5);

  EXPECT_EQ(format_number(32.0), "32");
  EXPECT_EQ(format_number(123.45), "123.45");
}

namespace {

void append_u32(std::string &out, const std::uint32_t value) {
  append_u16(out, static_cast<std::uint16_t>(value & 0xFFFF));
  append_u16(out, static_cast<std::uint16_t>(value >> 16));
}

/// A Font record body ([MS-XLS] 2.4.122).
std::string make_font(const std::uint16_t dy_height, const std::uint16_t grbit,
                      const std::uint16_t icv, const std::uint16_t bls,
                      const std::uint8_t uls, const std::string &name) {
  std::string body;
  append_u16(body, dy_height);
  append_u16(body, grbit);
  append_u16(body, icv);
  append_u16(body, bls);
  append_u16(body, 0); // sss
  body.push_back(static_cast<char>(uls));
  body += std::string("\0\0\0", 3); // bFamily, bCharSet, unused3
  body.push_back(static_cast<char>(name.size()));
  body.push_back('\x00'); // flags: compressed
  body += name;
  return body;
}

/// An XF record body ([MS-XLS] 2.4.353): font index plus the fill pattern and
/// foreground fill color of its CellXF.
std::string make_xf(const std::uint16_t ifnt, const std::uint32_t fls,
                    const std::uint16_t icv_fore) {
  std::string body;
  append_u16(body, ifnt);
  append_u16(body, 0);                       // ifmt
  append_u16(body, 0);                       // fLocked..ixfParent
  append_u16(body, 0);                       // alc..trot
  append_u16(body, 0);                       // cIndent..fAtr*
  append_u16(body, 0);                       // border styles
  append_u16(body, 0);                       // icvLeft, icvRight, grbitDiag
  append_u32(body, fls << 26);               // icvTop..dgDiag, fls
  append_u16(body, icv_fore | (0x41u << 7)); // icvFore, icvBack = default
  return body;
}

/// A Label record body ([MS-XLS] 2.4.148): an inline-string cell.
std::string make_label(const std::uint16_t row, const std::uint16_t column,
                       const std::uint16_t ixfe, const std::string &text) {
  std::string body;
  append_u16(body, row);
  append_u16(body, column);
  append_u16(body, ixfe);
  append_u16(body, static_cast<std::uint16_t>(text.size()));
  body.push_back('\x00'); // flags: compressed
  body += text;
  return body;
}

std::string make_bof(const std::uint16_t dt) {
  std::string body;
  append_u16(body, 0x0600); // vers: BIFF8
  append_u16(body, dt);
  return body;
}

/// One-sheet workbook stream: `globals` records are wrapped with BOF/
/// BoundSheet8/EOF, the sheet substream holds the given `cells`.
std::string make_workbook(const std::string &globals,
                          const std::vector<std::string> &cells) {
  const auto build_globals = [&](const std::uint32_t sheet_offset) {
    std::string result;
    append_record(result, 0x0809 /* BOF */, make_bof(0x0005));
    result += globals;
    std::string boundsheet;
    append_u32(boundsheet, sheet_offset);
    boundsheet.push_back('\x00'); // visible
    boundsheet.push_back('\x00'); // worksheet
    boundsheet.push_back('\x06');
    boundsheet.push_back('\x00'); // name: compressed
    boundsheet += "Sheet1";
    append_record(result, 0x0085 /* BoundSheet8 */, boundsheet);
    append_record(result, 0x000A /* EOF */, "");
    return result;
  };

  std::string sheet;
  append_record(sheet, 0x0809 /* BOF */, make_bof(0x0010));
  for (const std::string &cell : cells) {
    append_record(sheet, 0x0204 /* Label */, cell);
  }
  append_record(sheet, 0x000A /* EOF */, "");

  return build_globals(static_cast<std::uint32_t>(build_globals(0).size())) +
         sheet;
}

Document open_workbook(const std::string &workbook) {
  auto files = std::make_shared<internal::VirtualFilesystem>();
  files->copy(std::make_shared<internal::MemoryFile>(workbook),
              internal::AbsPath("/Workbook"));
  return Document(
      std::make_shared<internal::oldms::spreadsheet::Document>(files));
}

Text first_text(const Element cell) {
  return cell.first_child().first_child().as_text();
}

} // namespace

// Fonts and fills resolve through XF -> Font/palette ([MS-XLS] 2.4.353,
// 2.4.122): the cell's ixfe picks the XF, whose ifnt picks the Font (index 4
// is skipped and values above 4 are one-based, [MS-XLS] 2.5.129); colors use
// the default palette when no Palette record is present ([MS-XLS] 2.5.161).
TEST(OldMs, xls_cell_styles) {
  const std::string plain_font =
      make_font(200, 0, 0x7FFF /* automatic */, 400, 0, "Arial");
  // grbit: fItalic (bit 1) + fStrikeOut (bit 3); icv 0x11 = default palette
  // index 9 = 0x008000; single underline.
  const std::string fancy_font =
      make_font(320, 0x000A, 0x0011, 700, 1, "Comic Sans MS");

  std::string globals;
  for (int i = 0; i < 4; ++i) {
    append_record(globals, 0x0031 /* Font */, plain_font);
  }
  append_record(globals, 0x0031 /* Font */, fancy_font); // ifnt 5
  append_record(globals, 0x00E0 /* XF */, make_xf(0, 0, 0));
  // Solid fill; icvFore 0x0C = default palette index 4 = 0x0000FF.
  append_record(globals, 0x00E0 /* XF */, make_xf(5, 1, 0x0C));

  const Document document = open_workbook(make_workbook(
      globals, {make_label(0, 0, 0, "plain"), make_label(0, 1, 1, "fancy")}));

  const Sheet sheet = document.root_element().first_child().as_sheet();

  const TextStyle plain = first_text(sheet.cell(0, 0)).style();
  EXPECT_STREQ(plain.font_name, "Arial");
  EXPECT_EQ(plain.font_size, Measure("10pt"));
  EXPECT_EQ(plain.font_weight, FontWeight::normal);
  EXPECT_EQ(plain.font_style, FontStyle::normal);
  EXPECT_EQ(plain.font_underline, false);
  EXPECT_EQ(plain.font_line_through, false);
  EXPECT_FALSE(plain.font_color.has_value()); // automatic
  EXPECT_FALSE(sheet.cell_style(0, 0).background_color.has_value());

  const TextStyle fancy = first_text(sheet.cell(1, 0)).style();
  EXPECT_STREQ(fancy.font_name, "Comic Sans MS");
  EXPECT_EQ(fancy.font_size, Measure("16pt"));
  EXPECT_EQ(fancy.font_weight, FontWeight::bold);
  EXPECT_EQ(fancy.font_style, FontStyle::italic);
  EXPECT_EQ(fancy.font_underline, true);
  EXPECT_EQ(fancy.font_line_through, true);
  ASSERT_TRUE(fancy.font_color.has_value());
  EXPECT_EQ(fancy.font_color->rgb(), 0x008000);

  const auto fill = sheet.cell_style(1, 0).background_color;
  ASSERT_TRUE(fill.has_value());
  EXPECT_EQ(fill->rgb(), 0x0000FF);

  // Positions without a cell record stay unstyled.
  EXPECT_FALSE(sheet.cell_style(5, 5).background_color.has_value());
}

// A Palette record ([MS-XLS] 2.4.188) replaces the default palette for the
// icv values 0x08-0x3F.
TEST(OldMs, xls_palette_record) {
  std::string globals;
  append_record(globals, 0x0031 /* Font */,
                make_font(200, 0, 0x0008, 400, 0, "Arial"));
  append_record(globals, 0x00E0 /* XF */, make_xf(0, 0, 0));

  std::string palette;
  append_u16(palette, 56);                       // ccv
  palette += std::string("\x12\x34\x56\x00", 4); // rgColor[0] -> icv 0x08
  palette += std::string(std::size_t{55} * 4, '\x00');
  append_record(globals, 0x0092 /* Palette */, palette);

  const Document document =
      open_workbook(make_workbook(globals, {make_label(0, 0, 0, "x")}));

  const Sheet sheet = document.root_element().first_child().as_sheet();
  const TextStyle style = first_text(sheet.cell(0, 0)).style();
  ASSERT_TRUE(style.font_color.has_value());
  EXPECT_EQ(style.font_color->rgb(), 0x123456);
}

TEST(OldMs, xls_empty) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/xls/empty.xls"), *logger);

  EXPECT_EQ(document_file.file_type(), FileType::legacy_excel_worksheets);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::spreadsheet);

  std::size_t sheet_count = 0;
  for (const Element sheet_element : document.root_element().children()) {
    ASSERT_EQ(sheet_element.type(), ElementType::sheet);
    const Sheet sheet = sheet_element.as_sheet();
    EXPECT_EQ(sheet.name(), "Sheet1");
    const TableDimensions content = sheet.content(std::nullopt);
    EXPECT_EQ(content.rows, 0);
    EXPECT_EQ(content.columns, 0);
    ++sheet_count;
  }
  EXPECT_EQ(sheet_count, 1);
}

TEST(OldMs, xls_file_example_10) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/xls/file_example_XLS_10.xls"),
      *logger);

  EXPECT_EQ(document_file.file_type(), FileType::legacy_excel_worksheets);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::spreadsheet);

  std::vector<Element> sheets;
  for (const Element sheet_element : document.root_element().children()) {
    ASSERT_EQ(sheet_element.type(), ElementType::sheet);
    sheets.push_back(sheet_element);
  }
  ASSERT_EQ(sheets.size(), 1);

  const Sheet sheet = sheets.front().as_sheet();
  EXPECT_EQ(sheet.name(), "Sheet1");

  const TableDimensions dimensions = sheet.dimensions();
  EXPECT_EQ(dimensions.rows, 11);
  EXPECT_EQ(dimensions.columns, 8);

  const TableDimensions content = sheet.content(std::nullopt);
  EXPECT_EQ(content.rows, 10);
  EXPECT_EQ(content.columns, 8);

  // header row (SST strings)
  EXPECT_EQ(collect_text(sheet.cell(1, 0)), "First Name");
  EXPECT_EQ(collect_text(sheet.cell(7, 0)), "Id");
  // strings and numbers (RK/MULRK)
  EXPECT_EQ(collect_text(sheet.cell(1, 1)), "Dulce");
  EXPECT_EQ(collect_text(sheet.cell(4, 1)), "United States");
  EXPECT_EQ(collect_text(sheet.cell(5, 1)), "32");
  EXPECT_EQ(collect_text(sheet.cell(6, 1)), "15/10/2017");
  EXPECT_EQ(collect_text(sheet.cell(7, 9)), "6548");
  EXPECT_EQ(collect_text(sheet.cell(2, 9)), "Weiland");
}

// The 5000-row variant has a shared string table well beyond the 8224-byte
// record limit, so it exercises the SST CONTINUE handling on a real file.
TEST(OldMs, xls_file_example_5000) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/xls/file_example_XLS_5000.xls"),
      *logger);

  const Document document = document_file.document();

  const Sheet sheet = document.root_element().first_child().as_sheet();
  const TableDimensions content = sheet.content(std::nullopt);
  EXPECT_EQ(content.rows, 5001);
  EXPECT_EQ(content.columns, 8);

  EXPECT_EQ(collect_text(sheet.cell(1, 5000)), "Rasheeda");
  EXPECT_EQ(collect_text(sheet.cell(2, 5000)), "Alkire");
  EXPECT_EQ(collect_text(sheet.cell(7, 5000)), "6125");
}

TEST(OldMs, ppt_empty) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ppt/empty.ppt"), *logger);

  EXPECT_EQ(document_file.file_type(),
            FileType::legacy_powerpoint_presentation);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::presentation);

  std::size_t slide_count = 0;
  for (const Element slide : document.root_element().children()) {
    EXPECT_EQ(slide.type(), ElementType::slide);
    ++slide_count;
  }
  EXPECT_EQ(slide_count, 1);
}

TEST(OldMs, ppt_style_various) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ppt/style-various-1.ppt"), *logger);

  EXPECT_EQ(document_file.file_type(),
            FileType::legacy_powerpoint_presentation);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::presentation);

  // The text boxes (frames) of each slide, in shape order.
  std::vector<std::vector<Element>> slides;
  for (const Element slide : document.root_element().children()) {
    EXPECT_EQ(slide.type(), ElementType::slide);
    std::vector<Element> frames;
    for (const Element child : slide.children()) {
      ASSERT_EQ(child.type(), ElementType::frame);
      frames.push_back(child);
    }
    ASSERT_FALSE(frames.empty()); // every slide has at least its title box
    slides.push_back(std::move(frames));
  }
  ASSERT_EQ(slides.size(), 8);

  // Every frame is positioned (anchored to the page, all four measures
  // present).
  std::size_t total_frames = 0;
  for (const std::vector<Element> &frames : slides) {
    total_frames += frames.size();
    for (const Element &element : frames) {
      const Frame frame = element.as_frame();
      EXPECT_EQ(frame.anchor_type(), AnchorType::at_page);
      EXPECT_TRUE(frame.x().has_value());
      EXPECT_TRUE(frame.y().has_value());
      EXPECT_TRUE(frame.width().has_value());
      EXPECT_TRUE(frame.height().has_value());
    }
  }
  EXPECT_EQ(total_frames, 12);

  // The first frame of each slide is its title "titleN…", in order.
  for (std::size_t i = 0; i < slides.size(); ++i) {
    const std::string title = collect_text(slides[i].front());
    EXPECT_EQ(title.rfind("title" + std::to_string(i + 1), 0), 0u)
        << "slide " << i << " title: " << title;
  }

  // Slide 0 is a title + subtitle box, at different vertical positions.
  ASSERT_EQ(slides[0].size(), 2);
  EXPECT_EQ(collect_text(slides[0][0]), "title1");
  EXPECT_EQ(collect_text(slides[0][1]), "subtitle");
  EXPECT_NE(slides[0][0].as_frame().y(), slides[0][1].as_frame().y());
}
