#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

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
                                      HtmlConfig(), Logger::null());
  return out.str();
}

} // namespace

namespace {

std::vector<FileType> detect(const std::string &content) {
  return internal::open_strategy::list_file_types(
      std::make_shared<internal::MemoryFile>(content), Logger::null());
}

} // namespace

/// An svg has no signature - it is xml, and only the root element separates it
/// from any other xml. So `magic` does not guess it; the open strategy parses
/// the file and reports every layer it verified, least specific first.
TEST(image_file, svg_is_detected_by_parsing_it) {
  EXPECT_EQ(detect(R"(<?xml version="1.0"?><svg xmlns="x"/>)"),
            (std::vector{FileType::text_file, FileType::xml,
                         FileType::scalable_vector_graphics}));

  // a namespace prefix binds the root element just the same
  EXPECT_EQ(detect(R"(<svg:svg xmlns:svg="x"/>)"),
            (std::vector{FileType::text_file, FileType::xml,
                         FileType::scalable_vector_graphics}));

  // a prologue of any length is the parser's problem, not a scanner's
  EXPECT_EQ(detect("<!-- " + std::string(8000, 'c') + " -->\n<svg/>"),
            (std::vector{FileType::text_file, FileType::xml,
                         FileType::scalable_vector_graphics}));
}

/// The root element is what separates the two, so xml that is not an svg stops
/// at xml - including the two that a head scanner would most easily confuse.
TEST(image_file, xml_that_is_not_an_svg_stops_at_xml) {
  for (const std::string content :
       {R"(<html><body><svg/></body></html>)",
        R"(<?xml version="1.0"?><office:document xmlns:svg="x"/>)"}) {
    EXPECT_EQ(detect(content),
              (std::vector{FileType::text_file, FileType::xml}))
        << content;
  }

  // not xml at all
  EXPECT_EQ(detect("just some text"), (std::vector{FileType::text_file}));
}

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

/// An image inside a document is labelled by its own type, the same as the
/// image page labels a standalone one.
TEST(image_file, an_embedded_image_is_named_by_its_own_mime_type) {
  EXPECT_NE(image_src(svg_file()).find("data:image/svg+xml;base64,"),
            std::string::npos);
  EXPECT_NE(image_src(png_file()).find("data:image/png;base64,"),
            std::string::npos);
  EXPECT_NE(image_src(ico_file()).find("data:image/vnd.microsoft.icon;base64,"),
            std::string::npos);
}

/// A file nothing could name still goes out - as a guess, since there is no
/// type to read a mime type from.
TEST(image_file, an_unrecognised_image_falls_back_to_a_guess) {
  std::ostringstream out;
  internal::html::translate_image_src(image_file("not an image at all"), out,
                                      HtmlConfig(), Logger::null());

  EXPECT_NE(out.str().find("data:image/jpg;base64,"), std::string::npos);
}
