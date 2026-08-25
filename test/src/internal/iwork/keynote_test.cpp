#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_file.hpp>

#include <internal/iwork/iwork_element_util.hpp>
#include <internal/iwork/iwork_test_util.hpp>
#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;
using odr::test::TestData;
namespace builder = odr::test::iwork;
namespace iwork = odr::internal::iwork;

namespace {

/// The text of one frame. Every child of a frame is a paragraph, which the
/// shared reader does not assert, so state it here.
std::string frame_text(const Element frame) {
  for (const Element paragraph : frame.children()) {
    EXPECT_EQ(paragraph.type(), ElementType::paragraph);
  }
  return builder::element_text(frame);
}

/// The text of every frame of every slide, one vector per slide.
std::vector<std::vector<std::string>> slides(const Element root) {
  std::vector<std::vector<std::string>> result;

  for (const Element slide : root.children()) {
    EXPECT_EQ(slide.type(), ElementType::slide);

    std::vector<std::string> frames;
    for (const Element frame : slide.children()) {
      EXPECT_EQ(frame.type(), ElementType::frame);
      frames.push_back(frame_text(frame));
    }
    result.push_back(std::move(frames));
  }

  return result;
}

Document keynote_document(
    const std::vector<std::vector<builder::SlideBox>> &boxes,
    const std::optional<builder::SlideSize> &size = builder::SlideSize{}) {
  return Document(std::make_shared<iwork::Document>(
      FileType::iwork_keynote, builder::keynote_package(boxes, size)));
}

} // namespace

TEST(IworkKeynote, is_detected_by_content) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);
  const std::string path =
      TestData::test_file_path("odr-public/key/style-various-1.key");

  EXPECT_THAT(list_file_types(path, logger),
              testing::Contains(FileType::iwork_keynote));

  const DecodedFile file(path, logger);
  EXPECT_EQ(file.file_type(), FileType::iwork_keynote);
  EXPECT_EQ(file.file_category(), FileCategory::document);
  EXPECT_EQ(file.as_document_file().document_type(),
            DocumentType::presentation);
}

// A deck with one blank slide must come back with one empty slide rather than
// throw: `empty.key`'s title and body placeholders hold no text and are not in
// the slide's drawable list at all.
TEST(IworkKeynote, empty) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/key/empty.key"), logger);
  EXPECT_EQ(document_file.file_type(), FileType::iwork_keynote);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::presentation);
  EXPECT_FALSE(document.is_editable());
  EXPECT_FALSE(document.is_savable(false));

  EXPECT_EQ(slides(document.root_element()),
            (std::vector<std::vector<std::string>>{{}}));
}

TEST(IworkKeynote, slide_text) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/key/style-various-1.key"), logger);

  const Document document = document_file.document();
  const std::vector<std::vector<std::string>> text =
      slides(document.root_element());

  ASSERT_EQ(text.size(), 4);
  EXPECT_EQ(text[0], (std::vector<std::string>{"Presentation Title",
                                               "A subtitle for the deck"}));
  EXPECT_EQ(text[1],
            (std::vector<std::string>{
                "Bullets", "First bullet\nSecond bullet\nThird bullet"}));
  // the slide's body placeholder is empty, so it is not on the slide at all
  EXPECT_EQ(text[2], (std::vector<std::string>{"Centred Title"}));
  // the table is a drawable this stage does not read; the text box beside it
  // is
  EXPECT_EQ(text[3],
            (std::vector<std::string>{"A free text box\non a blank slide"}));
}

TEST(IworkKeynote, slides_are_named_in_presentation_order) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/key/style-various-1.key"),
      Logger::null());

  const Document document = document_file.document();

  std::vector<std::string> names;
  for (const Element slide : document.root_element().children()) {
    names.push_back(slide.as_slide().name());
  }

  EXPECT_EQ(names, (std::vector<std::string>{"Slide 1", "Slide 2", "Slide 3",
                                             "Slide 4"}));
}

// The show archive carries the slide size in points; the fixtures are the
// 1024x768 Keynote has defaulted to since the 13 era.
TEST(IworkKeynote, slide_page_layout_comes_from_the_show) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/key/empty.key"), Logger::null());

  const Document document = document_file.document();
  const Slide slide = (*document.root_element().children().begin()).as_slide();
  const PageLayout layout = slide.page_layout();

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "1024pt");
  EXPECT_EQ(layout.height->to_string(), "768pt");
}

TEST(IworkKeynote, a_text_box_is_a_frame_where_the_geometry_puts_it) {
  const Document document = keynote_document(
      {{builder::SlideBox{.text = "boxed\r",
                          .paragraphs = std::vector<std::uint64_t>{0},
                          .x = 100,
                          .y = 129,
                          .width = 824,
                          .height = 260}}});

  const Element slide = *document.root_element().children().begin();
  const Frame frame = (*slide.children().begin()).as_frame();

  EXPECT_EQ(frame.anchor_type(), AnchorType::at_page);
  ASSERT_TRUE(frame.x().has_value());
  ASSERT_TRUE(frame.y().has_value());
  ASSERT_TRUE(frame.width().has_value());
  ASSERT_TRUE(frame.height().has_value());
  EXPECT_EQ(frame.x()->to_string(), "100pt");
  EXPECT_EQ(frame.y()->to_string(), "129pt");
  EXPECT_EQ(frame.width()->to_string(), "824pt");
  EXPECT_EQ(frame.height()->to_string(), "260pt");
}

// A box flush against the left or top edge states an offset of zero, which is
// not the absent measure an unsized box reports.
TEST(IworkKeynote, a_text_box_against_the_slide_edge_is_at_zero) {
  const Document document = keynote_document(
      {{builder::SlideBox{.text = "flush",
                          .paragraphs = std::vector<std::uint64_t>{0},
                          .x = 0,
                          .y = 0,
                          .width = 824,
                          .height = 260}}});

  const Element slide = *document.root_element().children().begin();
  const Frame frame = (*slide.children().begin()).as_frame();

  ASSERT_TRUE(frame.x().has_value());
  ASSERT_TRUE(frame.y().has_value());
  EXPECT_EQ(frame.x()->to_string(), "0pt");
  EXPECT_EQ(frame.y()->to_string(), "0pt");
}

// Keynote stores a zero size for a box that grows with its text, which is not
// a box of zero height — report no measure and let the content decide.
TEST(IworkKeynote, a_text_box_that_autosizes_reports_no_size) {
  const Document document = keynote_document({{builder::SlideBox{
      .text = "grows", .paragraphs = std::nullopt, .x = 478, .y = 384}}});

  const Element slide = *document.root_element().children().begin();
  const Frame frame = (*slide.children().begin()).as_frame();

  EXPECT_TRUE(frame.x().has_value());
  EXPECT_FALSE(frame.width().has_value());
  EXPECT_FALSE(frame.height().has_value());
}

// The size is the show's, and a show that carries none leaves the layout to
// the renderer rather than laying a slide out at zero.
TEST(IworkKeynote, a_show_without_a_size_has_no_page_layout) {
  const Document document =
      keynote_document({{builder::SlideBox{.text = "sizeless"}}}, std::nullopt);

  const Slide slide = (*document.root_element().children().begin()).as_slide();
  const PageLayout layout = slide.page_layout();

  EXPECT_FALSE(layout.width.has_value());
  EXPECT_FALSE(layout.height.has_value());
}

// A placeholder wraps the same shape a free text box is, one level deeper.
TEST(IworkKeynote, a_placeholder_reads_like_a_text_box) {
  // assigned rather than designated: gcc rejects an initializer that steps
  // over the geometry to reach the flag behind it
  builder::SlideBox box{.text = "titled"};
  box.placeholder = true;

  const Document document = keynote_document({{box}});

  EXPECT_EQ(slides(document.root_element()),
            (std::vector<std::vector<std::string>>{{"titled"}}));
}

// Keynote ends a paragraph with `\r` where Pages ends it with `\n`; the run
// table says where the next one starts either way.
TEST(IworkKeynote, a_carriage_return_ends_a_paragraph) {
  const Document document = keynote_document({{builder::SlideBox{
      .text = "one\rtwo", .paragraphs = std::vector<std::uint64_t>{0, 4}}}});

  const Element slide = *document.root_element().children().begin();
  EXPECT_EQ(frame_text(*slide.children().begin()), "one\ntwo");
}

TEST(IworkKeynote, a_deck_without_slides_has_an_empty_root) {
  const Document document = keynote_document({});

  EXPECT_EQ(document.root_element().children().begin(),
            document.root_element().children().end());
}

// A package whose root archive is type 1 is a Keynote one only when it holds
// slide components — Numbers numbers its root archive the same, so one
// without them reads as a `.numbers`.
TEST(IworkKeynote, a_root_archive_without_slide_components_is_numbers) {
  const auto files =
      builder::pages_package(builder::text_storage("", std::nullopt),
                             builder::types::archive_type::app_document);

  EXPECT_EQ(iwork::IworkFile{files}.file_type(), FileType::iwork_numbers);
}

// A drawable list may name the same object any number of times, and every
// repeat expands into fresh elements holding a fresh copy of its text — a few
// kilobytes of references would otherwise expand without bound. The parse
// throws what every caller already handles instead.
TEST(IworkKeynote, a_repeated_drawable_is_capped_by_the_text_it_copies) {
  builder::SlideBox box{.text = std::string(1u << 20, 'a'),
                        .paragraphs = std::vector<std::uint64_t>{0}};
  box.repeats = 128;

  EXPECT_THAT([&] { keynote_document({{box}}); },
              testing::ThrowsMessage<std::runtime_error>(
                  testing::HasSubstr("too much text")));
}

TEST(IworkKeynote, a_repeated_drawable_is_capped_by_the_elements_it_builds) {
  builder::SlideBox box{.text = "short",
                        .paragraphs = std::vector<std::uint64_t>{0}};
  box.repeats = 600'000;

  EXPECT_THAT([&] { keynote_document({{box}}); },
              testing::ThrowsMessage<std::runtime_error>(
                  testing::HasSubstr("too many elements")));
}

// The cap is far above what a deck reaches, so a list that does name one
// drawable more than once is still read as it is written.
TEST(IworkKeynote, a_drawable_named_more_than_once_is_read_every_time) {
  builder::SlideBox box{.text = "again",
                        .paragraphs = std::vector<std::uint64_t>{0}};
  box.repeats = 3;

  EXPECT_EQ(
      slides(keynote_document({{box}}).root_element()),
      (std::vector<std::vector<std::string>>{{"again", "again", "again"}}));
}

// `KN.DocumentArchive` and `TN.DocumentArchive` are both type 1, so the real
// `.numbers` fixtures are what pins the component list telling the two apart.
TEST(IworkKeynote, a_numbers_package_is_not_keynote) {
  for (const char *name : {"empty.numbers", "style-various-1.numbers"}) {
    const std::string path =
        TestData::test_file_path(std::string("odr-public/numbers/") + name);

    EXPECT_THAT(list_file_types(path, Logger::null()),
                testing::Not(testing::Contains(FileType::iwork_keynote)));
    EXPECT_THROW(std::ignore =
                     open(path, FileType::iwork_keynote, Logger::null()),
                 UnknownFileType);
  }
}
