#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/magic.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <tuple>

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

/// What an encoder actually writes, rather than the signature a table says it
/// should: a jpeg names itself with whichever application marker it happens to
/// carry, and a webp only after its RIFF form tag.
TEST(magic, images) {
  for (const auto &[path, type, mimetype] :
       {std::tuple{"odr-public/png/tango-example-icons.png",
                   FileType::portable_network_graphics, "image/png"},
        std::tuple{"odr-public/jpg/fantastic-landscape.jpg", FileType::jpeg,
                   "image/jpeg"},
        std::tuple{"odr-public/gif/knights-tour.gif",
                   FileType::graphics_interchange_format, "image/gif"},
        std::tuple{"odr-public/bmp/tango-example-icons.bmp",
                   FileType::bitmap_image_file, "image/bmp"},
        std::tuple{"odr-public/webp/lorine-niedecker.webp", FileType::webp,
                   "image/webp"}}) {
    const File file(TestData::test_file_path(path));

    EXPECT_EQ(magic::file_type(*file.impl()), type) << path;
    EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
              mimetype)
        << path;
  }
}

/// An svg carries no signature at all - it is named by parsing it, and only the
/// open strategy does that.
TEST(magic, svg) {
  for (const std::string path :
       {"odr-public/svg/rotating-snakes.svg",
        "odr-public/svg/civitas-schinesghe-emblem.svg"}) {
    const File file(TestData::test_file_path(path));

    EXPECT_EQ(magic::file_type(*file.impl()), FileType::unknown) << path;
    EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
              "image/svg+xml")
        << path;
  }
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
