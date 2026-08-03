#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/magic.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <sstream>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

// the mimetype comes from the open strategy: the signature table names the
// container, and opening it names what is inside

TEST(magic, odt) {
  const File file(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::zip);

  EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
            "application/vnd.oasis.opendocument.text");
}

TEST(magic, doc) {
  const File file(TestData::test_file_path("odr-public/doc/empty.doc"));
  EXPECT_EQ(magic::file_type(*file.impl()),
            FileType::compound_file_binary_format);

  EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
            "application/msword");
}

TEST(magic, svm) {
  const File file(TestData::test_file_path("odr-public/svm/chart-1.svm"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::starview_metafile);

  EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
            "application/x-starview-metafile");
}

TEST(magic, odf) {
  const File file(TestData::test_file_path("odr-private/pdf/sample.pdf"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::portable_document_format);

  EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
            "application/pdf");
}

TEST(magic, wpd) {
  const File file(
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd"));
  EXPECT_EQ(magic::file_type(*file.impl()), FileType::word_perfect);

  EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
            "application/vnd.wordperfect");
}

namespace {

FileType detect(const std::string &head) {
  std::istringstream in(head);
  return magic::file_type(in);
}

} // namespace

// The media formats are named from their head alone and never opened, so a
// synthetic signature is the whole input the detection ever sees.
TEST(magic, riff_container) {
  using namespace std::string_literals;

  EXPECT_EQ(detect("RIFF\x24\x00\x00\x00WEBP"s), FileType::webp);
  EXPECT_EQ(detect("RIFF\x24\x00\x00\x00WAVE"s), FileType::waveform_audio);
  EXPECT_EQ(detect("RIFF\x24\x00\x00\x00"
                   "AVI "s),
            FileType::audio_video_interleave);
  // a RIFF holding something we do not name is still not a text file
  EXPECT_EQ(detect("RIFF\x24\x00\x00\x00RMID"s), FileType::unknown);
}

TEST(magic, iso_base_media_container) {
  using namespace std::string_literals;

  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypisom"s),
            FileType::mpeg4_video);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypmp42"s),
            FileType::mpeg4_video);
  EXPECT_EQ(detect("\x00\x00\x00\x14"
                   "ftypqt  "s),
            FileType::quicktime_video);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftyp3gp4"s),
            FileType::third_generation_partnership_video);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypheic"s),
            FileType::high_efficiency_image_format);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypmif1"s),
            FileType::high_efficiency_image_format);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypavif"s),
            FileType::av1_image_file_format);
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypM4A "s),
            FileType::mpeg4_audio);
  // an unknown brand is a video, which is what the container nearly always is
  EXPECT_EQ(detect("\x00\x00\x00\x18"
                   "ftypxxxx"s),
            FileType::mpeg4_video);
}

TEST(magic, standalone_media_signatures) {
  using namespace std::string_literals;

  EXPECT_EQ(detect("II*\x00"s), FileType::tagged_image_file_format);
  EXPECT_EQ(detect("MM\x00*"s), FileType::tagged_image_file_format);
  EXPECT_EQ(detect("\x1a\x45\xdf\xa3"s), FileType::matroska_video);
  EXPECT_EQ(detect("OggS"s), FileType::ogg_audio);
  EXPECT_EQ(detect("fLaC"s), FileType::free_lossless_audio_codec);
  EXPECT_EQ(detect("ID3\x04"s), FileType::mpeg_audio);
  EXPECT_EQ(detect("\xff\xfb\x90\x00"s), FileType::mpeg_audio);
  // still a jpeg, not an mp3 frame sync
  EXPECT_EQ(detect("\xff\xd8\xff\xdb"s), FileType::jpeg);
}

TEST(magic, image_signatures) {
  using namespace std::string_literals;

  EXPECT_EQ(detect("\x00\x00\x01\x00\x01\x00"s), FileType::windows_icon);
  EXPECT_EQ(detect("\x00\x00\x02\x00\x01\x00"s), FileType::windows_icon);
  EXPECT_EQ(detect("\x00\x00\x00\x0c"
                   "jP  \r\n\x87\n"s),
            FileType::jpeg_2000);
  EXPECT_EQ(detect("\xff\x4f\xff\x51"s), FileType::jpeg_2000);
  EXPECT_EQ(detect("\x00\x00\x00\x0c"
                   "JXL \r\n\x87\n"s),
            FileType::jpeg_xl);
  EXPECT_EQ(detect("\xff\x0a"s), FileType::jpeg_xl);
  EXPECT_EQ(detect("8BPS\x00\x01"s), FileType::photoshop_document);
  EXPECT_EQ(detect("\xd7\xcd\xc6\x9a"s), FileType::windows_metafile);
  EXPECT_EQ(detect("\x01\x00\x09\x00\x00\x03"s), FileType::windows_metafile);
  EXPECT_EQ(detect("\x02\x00\x09\x00\x00\x03"s), FileType::windows_metafile);

  // an enhanced metafile names itself at offset 40, not at the head
  const std::string emf = "\x01\x00\x00\x00"s + std::string(36, '\0') + " EMF";
  EXPECT_EQ(detect(emf), FileType::enhanced_metafile);
  EXPECT_EQ(detect("\x01\x00\x00\x00"s + std::string(36, '\0') + "junk"),
            FileType::unknown);

  // the neighbours these must not steal
  EXPECT_EQ(detect("\x00\x01\x00\x00"s), FileType::truetype_font);
  EXPECT_EQ(detect("\xff\xd8\xff\xdb"s), FileType::jpeg);
  EXPECT_EQ(detect("\xff\xfb\x90\x00"s), FileType::mpeg_audio);
}

TEST(magic, svg) {
  using namespace std::string_literals;

  EXPECT_EQ(detect("<svg xmlns=\"http://www.w3.org/2000/svg\"/>"s),
            FileType::scalable_vector_graphics);
  EXPECT_EQ(detect("<svg>"s), FileType::scalable_vector_graphics);
  EXPECT_EQ(detect("<?xml version=\"1.0\"?>\n<svg width=\"1\"/>"s),
            FileType::scalable_vector_graphics);
  EXPECT_EQ(detect("\xef\xbb\xbf<?xml version=\"1.0\"?>\n"
                   "<!-- drawn by hand -->\n"
                   "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"x\">\n"
                   "<svg:svg xmlns:svg=\"http://www.w3.org/2000/svg\"/>"s),
            FileType::scalable_vector_graphics);
}

/// A prologue has no length limit - a generated file can carry a licence
/// comment far longer than the signature head - so detection reads on until it
/// reaches the root element.
TEST(magic, svg_behind_a_long_prologue) {
  using namespace std::string_literals;

  const std::string comment = "<!-- " + std::string(8000, 'c') + " -->\n";
  EXPECT_EQ(detect(R"(<?xml version="1.0"?>)"
                   "\n" +
                   comment + "<svg/>"),
            FileType::scalable_vector_graphics);

  // the same length of prologue in front of something that is not an svg
  EXPECT_EQ(detect(comment + "<html/>"), FileType::unknown);

  // reading on is bounded: a comment that never ends is not an svg
  EXPECT_EQ(detect("<!-- " + std::string(200000, 'c')), FileType::unknown);
}

TEST(magic, not_svg) {
  using namespace std::string_literals;

  // an inline `<svg>` does not make an html page an image
  EXPECT_EQ(detect("<!DOCTYPE html>\n<html><body><svg/></body></html>"s),
            FileType::unknown);
  // a flat opendocument declares the svg namespace and is still not an svg
  EXPECT_EQ(detect("<?xml version=\"1.0\"?>\n<office:document "
                   "xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:"
                   "svg-compatible:1.0\"/>"s),
            FileType::unknown);
  // a prologue or a root element name that runs past the head tells us nothing
  EXPECT_EQ(detect("<?xml version=\"1.0\""s), FileType::unknown);
  EXPECT_EQ(detect("<svg"s), FileType::unknown);
  EXPECT_EQ(detect("   \n\t"s), FileType::unknown);
}

TEST(magic, short_stream) {
  // a file shorter than the head buffer may only be matched against what was
  // actually read from it, never against what happened to sit behind it
  std::istringstream empty("");
  EXPECT_EQ(magic::file_type(empty), FileType::unknown);

  std::istringstream one_byte("P");
  EXPECT_EQ(magic::file_type(one_byte), FileType::unknown);

  std::istringstream two_bytes("BM");
  EXPECT_EQ(magic::file_type(two_bytes), FileType::bitmap_image_file);
}
