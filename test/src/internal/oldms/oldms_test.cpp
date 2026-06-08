#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;

TEST(OldMs, test) {
  const auto path = TestData::test_file_path("odr-public/doc/11KB.doc");

  const ArchiveFile cbf_file =
      open(path, FileType::compound_file_binary_format).as_archive_file();
  const Archive cbf = cbf_file.archive();
  const Filesystem files = cbf.as_filesystem();

  for (const auto walker = files.file_walker("/"); !walker.end();
       walker.next()) {
    std::cout << walker.path() << std::endl;
  }

  const std::string word_document =
      internal::util::stream::read(*files.open("/WordDocument").stream());
  std::cout << "/WordDocument size " << word_document.size() << std::endl;

  const std::size_t fib_size = internal::oldms::text::determine_size_Fib(
      *files.open("/WordDocument").stream());
  std::cout << "Fib size " << fib_size << std::endl;

  const auto stream = files.open("/WordDocument").stream();
  internal::oldms::text::ParsedFib fib;
  internal::oldms::text::read(*stream, fib);

  const std::string tableStreamPath =
      fib.base.fWhichTblStm == 1 ? "/1Table" : "/0Table";

  const std::string table =
      internal::util::stream::read(*files.open(tableStreamPath).stream());
  std::cout << tableStreamPath << " size " << table.size() << std::endl;

  const auto table_stream = files.open(tableStreamPath).stream();
  std::cout << "Fib.fibRgFcLcb->clx.fc " << fib.fibRgFcLcb->clx.fc << std::endl;
  std::cout << "Fib.fibRgFcLcb->clx.lcb " << fib.fibRgFcLcb->clx.lcb
            << std::endl;
  table_stream->ignore(fib.fibRgFcLcb->clx.fc);
  const internal::oldms::text::CharacterIndex character_index =
      internal::oldms::text::read_character_index(*table_stream);
  for (const auto &entry : character_index) {
    const auto document_stream = files.open("/WordDocument").stream();
    document_stream->seekg(entry.data_offset);
    const std::string text = internal::oldms::text::read_string(
        *document_stream, entry.length_cp, entry.is_compressed);
    std::cout << "text " << text << std::endl;
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

TEST(OldMs, ppt_slides) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ppt/slides.ppt"), *logger);

  EXPECT_EQ(document_file.file_type(),
            FileType::legacy_powerpoint_presentation);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::presentation);

  std::vector<Element> slides;
  for (const Element slide : document.root_element().children()) {
    EXPECT_EQ(slide.type(), ElementType::slide);
    slides.push_back(slide);
  }
  ASSERT_EQ(slides.size(), 2);

  // Each slide is a set of positioned text boxes (frames), in shape order.
  const auto frames_of = [](const Element &slide) {
    std::vector<Element> frames;
    for (const Element child : slide.children()) {
      frames.push_back(child);
    }
    return frames;
  };
  const std::vector<Element> s0 = frames_of(slides[0]);
  const std::vector<Element> s1 = frames_of(slides[1]);
  ASSERT_EQ(s0.size(), 2);
  ASSERT_EQ(s1.size(), 2);

  // Every frame is positioned (anchored to the page, all four measures
  // present).
  for (const Element &element : {s0[0], s0[1], s1[0], s1[1]}) {
    EXPECT_EQ(element.type(), ElementType::frame);
    const Frame frame = element.as_frame();
    EXPECT_EQ(frame.anchor_type(), AnchorType::at_page);
    EXPECT_TRUE(frame.x().has_value());
    EXPECT_TRUE(frame.y().has_value());
    EXPECT_TRUE(frame.width().has_value());
    EXPECT_TRUE(frame.height().has_value());
  }

  // The two boxes on a slide sit at different vertical positions (title above
  // body).
  EXPECT_NE(s0[0].as_frame().y(), s0[1].as_frame().y());
  EXPECT_NE(s1[0].as_frame().y(), s1[1].as_frame().y());

  // Text per box, in shape order.
  EXPECT_EQ(collect_text(s0[0]), "Hello PowerPoint");
  EXPECT_EQ(collect_text(s0[1]), "First bullet\nSecond bullet");
  EXPECT_EQ(collect_text(s1[0]), "Slide Two Title");
  EXPECT_EQ(collect_text(s1[1]), "Content on slide two");
}
