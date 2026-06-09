#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/oldms/text/doc_io.hpp>

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
