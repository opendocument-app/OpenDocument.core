#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/magic.hpp>
#include <odr/internal/project_info.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <sstream>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

// the mimetype comes from the open strategy, and from libmagic only in a build
// that still asks for the deprecated option. both answers are asserted, because
// the two agree on everything but `svm` - and because the open strategy branch
// used to be unreachable in every job that runs the tests, and stayed broken
// for it

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

  // the one format the two disagree on, and the one where our own table is
  // right
  if (project_info::has_libmagic()) {
    EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
              "application/octet-stream");
  } else {
    EXPECT_EQ(magic::mimetype(file.disk_path().value(), Logger::null()),
              "application/x-starview-metafile");
  }
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
