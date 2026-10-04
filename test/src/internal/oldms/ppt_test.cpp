#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <limits>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_document.hpp>
#include <odr/internal/oldms/presentation/ppt_io.hpp>
#include <odr/internal/oldms/presentation/ppt_style.hpp>
#include <sstream>

#include <internal/oldms/oldms_test_util.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;
using odr::test::oldms::append_u16;
using odr::test::oldms::append_u32;
using odr::test::oldms::collect_text;

namespace {
namespace ppt = internal::oldms::presentation;

std::string ppt_record(const std::uint16_t type, const std::string &body,
                       const std::uint16_t flags = 0) {
  std::string record;
  append_u16(record, flags);
  append_u16(record, type);
  append_u32(record, static_cast<std::uint32_t>(body.size()));
  return record + body;
}

std::shared_ptr<internal::VirtualFilesystem>
ppt_files(const std::string &document_body, const std::string &slide_body = {},
          const std::string &directory_tail = {}) {
  std::string document =
      ppt_record(ppt::RT_DocumentContainer, document_body, 15);
  const auto slide_offset = static_cast<std::uint32_t>(document.size());
  document += ppt_record(ppt::RT_SlideContainer, slide_body, 15);
  const auto directory_offset = static_cast<std::uint32_t>(document.size());
  std::string directory;
  append_u32(directory, (2u << 20) | 1u);
  append_u32(directory, 0);
  append_u32(directory, slide_offset);
  document +=
      ppt_record(ppt::RT_PersistDirectoryAtom, directory + directory_tail);
  const auto edit_offset = static_cast<std::uint32_t>(document.size());
  std::string edit(12, '\0');
  append_u32(edit, directory_offset);
  append_u32(edit, 1);
  append_u32(edit, 3);
  edit.resize(28, '\0');
  document += ppt_record(ppt::RT_UserEditAtom, edit);
  std::string user;
  append_u32(user, 20);
  append_u32(user, ppt::current_user_token_plain);
  append_u32(user, edit_offset);
  user.resize(20, '\0');
  auto files = std::make_shared<internal::VirtualFilesystem>();
  files->copy(std::make_shared<internal::MemoryFile>(document),
              internal::AbsPath("/PowerPoint Document"));
  files->copy(std::make_shared<internal::MemoryFile>(
                  ppt_record(ppt::RT_CurrentUserAtom, user)),
              internal::AbsPath("/Current User"));
  return files;
}

} // namespace

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
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file =
      open(TestData::test_file_path("odr-public/ppt/empty.ppt"), {}, logger)
          .as_document_file();

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
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file =
      open(TestData::test_file_path("odr-public/ppt/style-various-1.ppt"), {},
           logger)
          .as_document_file();

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
  EXPECT_EQ(total_frames, 13); // 12 text boxes + 1 background picture

  // The first text frame of each slide is its title "titleN…", in order.
  for (std::size_t i = 0; i < slides.size(); ++i) {
    std::string title;
    for (const Element &frame : slides[i]) {
      title = collect_text(frame);
      if (!title.empty()) {
        break;
      }
    }
    EXPECT_EQ(title.rfind("title" + std::to_string(i + 1), 0), 0u)
        << "slide " << i << " title: " << title;
  }

  // Slide 5 ("title6 - background image") carries a full-slide background
  // picture: a leading frame holding an image element with the PNG bytes from
  // the "Pictures" stream.
  ASSERT_EQ(slides[5].size(), 2);
  const Element background = slides[5][0];
  EXPECT_EQ(collect_text(background), "");
  ASSERT_EQ(background.first_child().type(), ElementType::image);
  const Image image = background.first_child().as_image();
  EXPECT_TRUE(image.is_internal());
  ASSERT_TRUE(image.file().has_value());
  std::array<char, 4> magic{};
  image.file()->stream()->read(magic.data(), magic.size());
  EXPECT_EQ(std::string(magic.data() + 1, 3), "PNG");
  // A non-empty pseudo-path names the BLIP so the HTML renderer can emit a
  // distinct resource per picture when images are not embedded.
  EXPECT_EQ(image.href(), "Pictures/1.png");
  EXPECT_EQ(background.as_frame().x(), Measure("0in"));
  EXPECT_EQ(background.as_frame().y(), Measure("0in"));

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
  EXPECT_EQ(title.font_name, "Arial");
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

TEST(OldMs, ppt_text_bytes_require_complete_record) {
  using internal::oldms::presentation::read_raw_text_bytes;
  std::istringstream valid("abcNEXT");
  EXPECT_EQ(read_raw_text_bytes(valid, 3), "abc");
  EXPECT_EQ(valid.peek(), 'N');
  std::istringstream truncated("ab");
  EXPECT_THROW(
      read_raw_text_bytes(truncated, std::numeric_limits<std::uint32_t>::max()),
      std::runtime_error);
  std::istringstream throwing("ab");
  throwing.exceptions(std::ios::failbit | std::ios::badbit);
  EXPECT_THROW(read_raw_text_bytes(throwing, 3), std::ios_base::failure);
}

TEST(OldMs, ppt_rejects_invalid_record_boundaries) {
  EXPECT_NO_THROW(ppt::Document{ppt_files({})});
  EXPECT_THROW(ppt::Document{ppt_files({}, {}, "x")}, std::runtime_error);
  std::string missing_offsets;
  append_u32(missing_offsets, (2u << 20) | 3u);
  EXPECT_THROW(ppt::Document{ppt_files({}, {}, missing_offsets)},
               std::runtime_error);

  std::string blip =
      ppt_record(ppt::RT_OfficeArtBlipPNG, std::string(17, '\0'));
  blip.pop_back();
  const std::string fbse =
      ppt_record(ppt::RT_OfficeArtFBSE, std::string(36, '\0') + blip);
  const std::string store =
      ppt_record(ppt::RT_OfficeArtBStoreContainer, fbse, 15);
  const std::string drawing =
      ppt_record(ppt::RT_DrawingGroup,
                 ppt_record(ppt::RT_OfficeArtDggContainer, store, 15), 15);
  EXPECT_THROW(ppt::Document{ppt_files(drawing)}, std::runtime_error);
}

TEST(OldMs, ppt_text_nesting_is_bounded) {
  const auto files = [](const std::size_t depth) {
    std::string text = ppt_record(ppt::RT_TextBytesAtom, "visible");
    for (std::size_t i = 0; i < depth; ++i) {
      text = ppt_record(0x7777, text, 15);
    }
    const std::string shape =
        ppt_record(ppt::RT_OfficeArtSpContainer,
                   ppt_record(ppt::RT_OfficeArtClientTextbox, text), 15);
    const std::string slide = ppt_record(
        ppt::RT_Drawing,
        ppt_record(ppt::RT_OfficeArtDgContainer,
                   ppt_record(ppt::RT_OfficeArtSpgrContainer, shape, 15), 15),
        15);
    std::string persist;
    append_u32(persist, 2);
    return ppt_files(ppt_record(ppt::RT_SlideListWithText,
                                ppt_record(ppt::RT_SlidePersistAtom, persist),
                                15),
                     slide);
  };
  const Document shallow(std::make_shared<ppt::Document>(files(4)));
  EXPECT_EQ(collect_text(shallow.root_element()), "visible");
  EXPECT_THROW(ppt::Document{files(1024)}, std::runtime_error);
}

TEST(OldMs, ppt_style_runs_require_complete_coverage) {
  std::string body;
  append_u32(body, 3);
  append_u16(body, 0);
  append_u32(body, 0);
  append_u32(body, 3);
  append_u32(body, 0);
  EXPECT_EQ(ppt::parse_style_text_prop_atom(body, 3).size(), 1);
  EXPECT_THROW(ppt::parse_style_text_prop_atom(body, 2), std::runtime_error);
  EXPECT_THROW(ppt::parse_style_text_prop_atom(body, 4), std::runtime_error);
  body.pop_back();
  EXPECT_THROW(ppt::parse_style_text_prop_atom(body, 3), std::runtime_error);
}

TEST(OldMs, ppt_anchor_differences_do_not_overflow) {
  ppt::Document document(ppt_files({}));
  auto [id, element, frame] =
      document.element_registry().create_frame_element();
  frame.anchor = ppt::Anchor{std::numeric_limits<std::int32_t>::min(),
                             std::numeric_limits<std::int32_t>::min(),
                             std::numeric_limits<std::int32_t>::max(),
                             std::numeric_limits<std::int32_t>::max()};
  const Frame handle = Element(document.element_adapter(), id).as_frame();
  const Measure extent(4294967295.0 / ppt::master_units_per_inch,
                       DynamicUnit("in"));
  EXPECT_EQ(handle.width(), extent);
  EXPECT_EQ(handle.height(), extent);
}
