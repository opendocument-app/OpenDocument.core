#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/oldms/spreadsheet/xls_io.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>

#include <bit>
#include <cstdint>
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
