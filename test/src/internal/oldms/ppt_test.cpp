#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/oldms/presentation/ppt_io.hpp>

#include <internal/oldms/oldms_test_util.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;
using odr::test::oldms::append_u16;
using odr::test::oldms::append_u32;
using odr::test::oldms::collect_text;

// A StyleTextPropAtom body ([MS-PPT] 2.9.44): the paragraph-level runs are
// skipped (including their mask-dependent fields), the character-level runs
// map masks/CFStyle/fontRef/size/color onto TextCFRun.
TEST(OldMs, ppt_parse_style_text_prop_atom) {
  using internal::oldms::presentation::parse_style_text_prop_atom;
  using internal::oldms::presentation::TextCFRun;

  std::string body;
  // One paragraph run covering all 12 characters: count, indentLevel, then a
  // TextPFException with a bullet flag so a mask-dependent field is skipped.
  append_u32(body, 12);
  append_u16(body, 0);          // indentLevel
  append_u32(body, 0x00000001); // PFMasks: hasBullet
  append_u16(body, 0);          // bulletFlags
  // Character run 1: 6 characters, bold on + italic off.
  append_u32(body, 6);
  append_u32(body, 0x00000003); // CFMasks: bold | italic
  append_u16(body, 0x0001);     // CFStyle: bold set, italic clear
  // Character run 2: 6 characters, font 1, 32pt, explicit red.
  append_u32(body, 6);
  append_u32(body, 0x00070000); // CFMasks: typeface | size | color
  append_u16(body, 1);          // fontRef
  append_u16(body, 32);         // fontSize
  body += std::string("\xFF\x00\x00\xFE", 4); // ColorIndexStruct: sRGB red

  const std::vector<TextCFRun> runs = parse_style_text_prop_atom(body, 12);
  ASSERT_EQ(runs.size(), 2);

  EXPECT_EQ(runs[0].count, 6);
  EXPECT_EQ(runs[0].bold, true);
  EXPECT_EQ(runs[0].italic, false);
  EXPECT_FALSE(runs[0].underline.has_value());
  EXPECT_FALSE(runs[0].font_size.has_value());

  EXPECT_EQ(runs[1].count, 6);
  EXPECT_FALSE(runs[1].bold.has_value());
  EXPECT_EQ(runs[1].font_ref, 1);
  EXPECT_EQ(runs[1].font_size, 32);
  ASSERT_TRUE(runs[1].color.has_value());
  EXPECT_EQ(runs[1].color->rgb(), 0xFF0000u);
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

  // Character formatting from the StyleTextPropAtoms: each text run is a
  // styled span under its paragraph.
  const auto first_span = [](const Element frame) {
    const Element span = frame.first_child().first_child();
    EXPECT_EQ(span.type(), ElementType::span);
    return span.as_span();
  };

  // The title is 44pt Arial with an explicit black color.
  const TextStyle title = first_span(slides[0][0]).style();
  EXPECT_STREQ(title.font_name, "Arial");
  EXPECT_EQ(title.font_size, Measure("44pt"));
  ASSERT_TRUE(title.font_color.has_value());
  EXPECT_EQ(title.font_color->rgb(), 0x000000u);

  // Slide 6 ("title7 - link") carries an underlined blue 32pt hyperlink text.
  ASSERT_EQ(slides[6].size(), 2);
  EXPECT_EQ(collect_text(slides[6][1]), "https://www.google.at/");
  const TextStyle link = first_span(slides[6][1]).style();
  EXPECT_EQ(link.font_size, Measure("32pt"));
  EXPECT_EQ(link.font_underline, true);
  ASSERT_TRUE(link.font_color.has_value());
  EXPECT_EQ(link.font_color->rgb(), 0x0000FFu);
}
