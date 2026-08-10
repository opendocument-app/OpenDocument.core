#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/svg/svg_file.hpp>
#include <odr/internal/text/text_file.hpp>
#include <odr/internal/xml/xml_file.hpp>

#include <pugixml.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;
using testing::HasSubstr;

namespace {

constexpr const char *svg_open = R"(<svg xmlns="http://www.w3.org/2000/svg">)";

std::shared_ptr<svg::SvgFile> svg_file(const std::string &content) {
  return std::make_shared<svg::SvgFile>(std::make_shared<xml::XmlFile>(
      std::make_shared<text::TextFile>(File::from_memory(content).impl())));
}

} // namespace

TEST(SvgFile, an_svg_opens_as_an_image_that_knows_it_is_one) {
  const DecodedFile file(File::from_memory(std::string(svg_open) + "</svg>"));

  EXPECT_EQ(file.file_type(), FileType::scalable_vector_graphics);
  EXPECT_EQ(file.file_category(), FileCategory::image);
  EXPECT_EQ(file.file_meta().mimetype, "image/svg+xml");
  EXPECT_TRUE(file.is_image_file());
  EXPECT_FALSE(file.is_text_file());
  EXPECT_TRUE(file.capabilities().translate_html);
}

/// The xml parse plus the root element is what decides the type.
TEST(SvgFile, only_an_svg_opens_as_one) {
  EXPECT_THROW(std::ignore = svg_file("<a/>"), NoSvgFile);
  EXPECT_THROW(std::ignore = svg_file("not xml at all"), NoXmlFile);

  // `open` reports its own failure to find a reading, not the format's
  EXPECT_THROW(std::ignore =
                   open(File::from_memory("<a/>"),
                        FileType::scalable_vector_graphics, Logger::null()),
               UnknownFileType);
}

/// pugixml does not process namespaces, so a prefixed root has to be seen for
/// what it is.
TEST(SvgFile, a_prefixed_root_element_is_still_an_svg) {
  EXPECT_NO_THROW(std::ignore = svg_file(
                      R"(<s:svg xmlns:s="http://www.w3.org/2000/svg"/>)"));
}

/// Every layer the file was read through stays reachable, so what the parse
/// already did is not done again downstream.
TEST(SvgFile, the_layers_it_was_read_through_are_reachable) {
  const std::string content = std::string(svg_open) + "<rect/></svg>";
  const std::shared_ptr<svg::SvgFile> file = svg_file(content);

  EXPECT_EQ(file->text(), content);
  EXPECT_EQ(file->text_file()->text(), content);
  EXPECT_EQ(file->xml_file()->root_name(), "svg");
  EXPECT_STREQ(file->document().document_element().name(), "svg");
  EXPECT_EQ(file->file()->size(), content.size());
}

/// The encoding comes off the declaration, which is what the xml layer is for.
TEST(SvgFile, the_declared_encoding_is_what_it_is_decoded_with) {
  const std::shared_ptr<svg::SvgFile> file =
      svg_file(R"(<?xml version="1.0" encoding="ISO-8859-1"?>)" +
               std::string(svg_open) + "<title>\xe4</title></svg>");

  EXPECT_EQ(file->xml_file()->encoding(), TextEncoding::iso_8859_1);
  EXPECT_THAT(file->text(), HasSubstr("<title>ä</title>"));
}

/// An svg renders as the `<img>` data url every other image does - inside one
/// a browser renders svg in secure static mode, and nothing here has to scrub
/// the markup to earn that.
TEST(SvgFile, it_renders_as_an_image) {
  const HtmlService service = odr::html::translate(
      DecodedFile(svg_file(std::string(svg_open) + "<rect/></svg>")),
      HtmlConfig(), Logger::null());

  ASSERT_EQ(service.list_views().size(), 1);
  EXPECT_EQ(service.list_views().front().name(), "image");

  std::ostringstream out;
  service.write("image.html", out);

  EXPECT_THAT(out.str(), HasSubstr("<img"));
  EXPECT_THAT(out.str(), HasSubstr("data:image/svg+xml;base64,"));
}
