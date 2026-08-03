#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/image_file.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>

using namespace odr;

namespace {

/// Signature plus payload. Nothing past the signature is ever read - these
/// formats are named, not decoded.
File image_file(const std::string &content) {
  return File(std::make_shared<internal::MemoryFile>(content));
}

File svg_file() {
  return image_file(R"(<?xml version="1.0"?>)"
                    "\n"
                    R"(<svg xmlns="http://www.w3.org/2000/svg"/>)");
}

File ico_file() { return image_file(std::string("\x00\x00\x01\x00", 4) + "d"); }

File png_file() {
  return image_file(std::string("\x89PNG\r\n\x1a\n", 8) + "payload");
}

std::string cache_path(const std::string &name) {
  return (std::filesystem::current_path() / name).string();
}

std::string write_path(const HtmlService &service, const std::string &path) {
  std::ostringstream out;
  service.write(path, out);
  return out.str();
}

std::string image_src(const File &file) {
  std::ostringstream out;
  internal::html::translate_image_src(DecodedFile(file).as_image_file(), out,
                                      HtmlConfig());
  return out.str();
}

} // namespace

TEST(image_file, svg_is_detected_and_opens_as_an_image) {
  const DecodedFile file{svg_file()};

  EXPECT_EQ(file.file_type(), FileType::scalable_vector_graphics);
  EXPECT_EQ(file.file_category(), FileCategory::image);
  EXPECT_EQ(file.file_meta().mimetype, "image/svg+xml");
  EXPECT_TRUE(file.capabilities().translate_html);

  EXPECT_TRUE(file.is_image_file());
  EXPECT_FALSE(file.is_decodable());
  EXPECT_FALSE(file.is_text_file());
}

TEST(image_file, svg_translates_to_an_image_page) {
  const DecodedFile file{svg_file()};
  const HtmlService service =
      html::translate(file, cache_path("image_svg"), HtmlConfig());

  ASSERT_EQ(service.list_views().size(), 1);
  EXPECT_EQ(service.list_views().front().name(), "image");

  const std::string html = write_path(service, "image.html");
  EXPECT_NE(html.find("<img"), std::string::npos);
  EXPECT_NE(html.find("data:image/svg+xml;base64,"), std::string::npos);
}

TEST(image_file, ico_is_named_by_its_own_mime_type) {
  const DecodedFile file{ico_file()};

  EXPECT_EQ(file.file_type(), FileType::windows_icon);
  EXPECT_TRUE(file.is_image_file());

  const HtmlService service =
      html::translate(file, cache_path("image_ico"), HtmlConfig());
  const std::string html = write_path(service, "image.html");
  EXPECT_NE(html.find("data:image/vnd.microsoft.icon;base64,"),
            std::string::npos);
}

/// The image wrapper used to leave its meta empty while the media, font and svm
/// wrappers all filled theirs in, so a png reported no type and no mimetype.
TEST(image_file, every_image_reports_its_meta) {
  for (const File &file : {png_file(), ico_file(), svg_file()}) {
    const DecodedFile decoded{file};
    const FileMeta meta = decoded.file_meta();

    EXPECT_EQ(meta.type, decoded.file_type());
    EXPECT_EQ(meta.mimetype, mimetype_by_file_type(decoded.file_type()));
  }
}

/// An image inside a document keeps the `image/jpg` every reference output was
/// written with; svg cannot, because markup labelled `image/jpg` renders
/// nothing.
TEST(image_file, an_embedded_svg_is_the_only_one_that_is_named) {
  EXPECT_NE(image_src(svg_file()).find("data:image/svg+xml;base64,"),
            std::string::npos);
  EXPECT_NE(image_src(png_file()).find("data:image/jpg;base64,"),
            std::string::npos);
}
