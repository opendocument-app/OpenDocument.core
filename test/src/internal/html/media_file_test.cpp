#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>

using namespace odr;
using namespace odr::internal;

namespace {

/// Signature plus payload. Nothing past the signature is ever read — audio and
/// video are named, not decoded — so the payload only has to come back out
/// unchanged.
File media_file(const std::string &signature) {
  return File(std::make_shared<MemoryFile>(signature + "payload"));
}

File mp3_file() { return media_file("ID3\x04"); }

const std::string mp4_signature =
    std::string("\x00\x00\x00\x18", 4) + "ftypmp42";

File mp4_file() { return media_file(mp4_signature); }

std::string cache_path(const std::string &name) {
  return (std::filesystem::current_path() / name).string();
}

std::string write_path(const HtmlService &service, const std::string &path) {
  std::ostringstream out;
  service.write(path, out);
  return out.str();
}

} // namespace

TEST(media_file, audio_is_decoded_without_a_wrapper_type) {
  const DecodedFile file{mp3_file()};

  EXPECT_EQ(file.file_type(), FileType::mpeg_audio);
  EXPECT_EQ(file.file_category(), FileCategory::audio);
  EXPECT_EQ(file.file_meta().mimetype, "audio/mpeg");
  EXPECT_TRUE(file.capabilities().translate_html);

  EXPECT_FALSE(file.is_decodable());
  EXPECT_FALSE(file.is_text_file());
  EXPECT_FALSE(file.is_image_file());
  EXPECT_FALSE(file.is_document_file());
}

TEST(media_file, audio_translates_to_a_player) {
  const DecodedFile file{mp3_file()};
  const HtmlService service =
      html::translate(file, cache_path("media_audio"), HtmlConfig());

  ASSERT_EQ(service.list_views().size(), 1);
  EXPECT_EQ(service.list_views().front().name(), "audio");
  EXPECT_EQ(service.list_views().front().path(), "audio.html");

  const std::string html = write_path(service, "audio.html");
  EXPECT_NE(html.find("<audio"), std::string::npos);
  EXPECT_NE(html.find("src=\"audio.mp3\""), std::string::npos);
  EXPECT_NE(html.find("controls"), std::string::npos);
}

TEST(media_file, video_translates_to_a_player) {
  const DecodedFile file{mp4_file()};
  const HtmlService service =
      html::translate(file, cache_path("media_video"), HtmlConfig());

  ASSERT_EQ(service.list_views().size(), 1);
  EXPECT_EQ(service.list_views().front().name(), "video");

  const std::string html = write_path(service, "video.html");
  EXPECT_NE(html.find("<video"), std::string::npos);
  EXPECT_NE(html.find("playsinline"), std::string::npos);
  EXPECT_NE(html.find("src=\"video.mp4\""), std::string::npos);
}

/// The point of the resource: a video is served and copied as bytes, never
/// base64'd into the markup the way an image is.
TEST(media_file, the_media_is_served_as_a_resource) {
  const DecodedFile file{mp4_file()};
  const HtmlService service =
      html::translate(file, cache_path("media_resource"), HtmlConfig());

  EXPECT_TRUE(service.exists("video.mp4"));
  EXPECT_EQ(service.mimetype("video.mp4"), "video/mp4");
  EXPECT_EQ(write_path(service, "video.mp4"), mp4_signature + "payload");

  EXPECT_EQ(write_path(service, "video.html").find("base64"),
            std::string::npos);

  EXPECT_THROW(write_path(service, "nothing.mp4"), FileNotFound);
}

TEST(media_file, bring_offline_writes_the_media_next_to_the_page) {
  const std::string output_path = cache_path("media_offline");
  std::filesystem::remove_all(output_path);

  const DecodedFile file{mp4_file()};
  const HtmlService service =
      html::translate(file, cache_path("media_offline_cache"), HtmlConfig());
  const Html html = service.bring_offline(output_path);

  ASSERT_EQ(html.pages().size(), 1);
  EXPECT_TRUE(std::filesystem::exists(html.pages().front().path));

  const std::filesystem::path media =
      std::filesystem::path(output_path) / "video.mp4";
  ASSERT_TRUE(std::filesystem::exists(media));
  std::ifstream in(media, std::ios::binary);
  std::ostringstream copied;
  copied << in.rdbuf();
  EXPECT_EQ(copied.str(), mp4_signature + "payload");
}

/// `.mkv` and `.webm` are one file type, so the canonical name would serve a
/// webm as `video/x-matroska` and no browser would play it. The name it came
/// in under wins whenever the same type claims it.
TEST(media_file, a_webm_keeps_its_own_name_and_mime) {
  const std::filesystem::path directory = cache_path("media_webm");
  std::filesystem::create_directories(directory);

  for (const auto &[name, path, mime_type] :
       {std::tuple{"video.webm", "clip.webm", "video/webm"},
        std::tuple{"video.mkv", "clip.mkv", "video/x-matroska"},
        // an extension of another type is ignored, not trusted
        std::tuple{"video.mkv", "clip.mp4", "video/x-matroska"}}) {
    const std::filesystem::path file_path = directory / path;
    std::ofstream(file_path, std::ios::binary) << "\x1a\x45\xdf\xa3payload";

    const DecodedFile file{File(file_path.string())};
    ASSERT_EQ(file.file_type(), FileType::matroska_video) << path;

    const HtmlService service =
        html::translate(file, cache_path("media_webm_cache"), HtmlConfig());

    EXPECT_TRUE(service.exists(name)) << path;
    EXPECT_EQ(service.mimetype(name), mime_type) << path;
    EXPECT_NE(write_path(service, "video.html")
                  .find(std::string("src=\"") + name + "\""),
              std::string::npos)
        << path;
  }
}

/// The image formats that arrived with the media types have no decoder either,
/// but they are images, so they take the image page rather than a player.
TEST(media_file, webp_opens_as_an_image) {
  const File file(std::make_shared<MemoryFile>(
      std::string("RIFF\x24\x00\x00\x00WEBPdata", 16)));
  const DecodedFile decoded{file};

  EXPECT_EQ(decoded.file_type(), FileType::webp);
  EXPECT_TRUE(decoded.is_image_file());

  const HtmlService service =
      html::translate(decoded, cache_path("media_webp"), HtmlConfig());
  ASSERT_EQ(service.list_views().size(), 1);

  // named, not guessed: a browser that honours the data URL's type has to be
  // told this is webp and not the `image/jpg` a document's images go out as
  const std::string html = write_path(service, "image.html");
  EXPECT_NE(html.find("<img"), std::string::npos);
  EXPECT_NE(html.find("data:image/webp;base64,"), std::string::npos);
}
