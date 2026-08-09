#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/html/image_file.hpp>

#include <odr/internal/svg/svg_file.hpp>
#include <odr/internal/text/text_file.hpp>
#include <odr/internal/xml/xml_file.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;
using testing::HasSubstr;
using testing::Not;

namespace {

constexpr const char *svg_open = R"(<svg xmlns="http://www.w3.org/2000/svg">)";

std::shared_ptr<svg::SvgFile> svg_file(const std::string &content) {
  return std::make_shared<svg::SvgFile>(std::make_shared<xml::XmlFile>(
      std::make_shared<text::TextFile>(File::from_memory(content).impl())));
}

std::string svg_html(const std::string &body) {
  const HtmlService service =
      odr::html::translate(DecodedFile(svg_file(svg_open + body + "</svg>")),
                           HtmlConfig(), Logger::null());

  std::ostringstream out;
  service.write("image.html", out);
  return out.str();
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

TEST(SvgHtml, the_markup_goes_into_the_page) {
  const std::string html = svg_html(R"(<rect width="10" height="10"/>)");

  EXPECT_THAT(html, HasSubstr(R"(<div class="odr-svg"><svg)"));
  EXPECT_THAT(html, HasSubstr(R"(<rect width="10" height="10")"));
  // and not as the base64 the image service would write
  EXPECT_THAT(html, Not(HasSubstr("data:image/svg+xml;base64")));
}

TEST(SvgHtml, the_page_forbids_script_outright) {
  EXPECT_THAT(svg_html(""),
              HasSubstr(R"(<meta http-equiv="Content-Security-Policy" )"
                        R"(content="script-src 'none'"/>)"));
}

TEST(SvgHtml, a_script_element_does_not_reach_the_page) {
  const std::string html = svg_html("<script>evil</script><rect/>");

  EXPECT_THAT(html, Not(HasSubstr("evil")));
  EXPECT_THAT(html, HasSubstr("<rect/>"));
}

/// The html parser lowercases element names in foreign content, so `<SCRIPT>`
/// would arrive in the page as a script element.
TEST(SvgHtml, an_uppercased_script_element_does_not_either) {
  EXPECT_THAT(svg_html("<SCRIPT>evil</SCRIPT>"), Not(HasSubstr("evil")));
  EXPECT_THAT(svg_html("<foreignObject><b>x</b></foreignObject>"),
              Not(HasSubstr("foreignObject")));
}

TEST(SvgHtml, event_handlers_are_stripped) {
  EXPECT_THAT(svg_html(R"(<rect onload="evil" width="1"/>)"),
              Not(HasSubstr("evil")));
  EXPECT_THAT(svg_html(R"(<rect ONCLICK="evil"/>)"), Not(HasSubstr("evil")));
  // and the element itself survives
  EXPECT_THAT(svg_html(R"(<rect onload="evil" width="1"/>)"),
              HasSubstr(R"(<rect width="1"/>)"));
}

/// A reference is kept only where it goes nowhere — same document, or an image
/// carried in the url itself.
TEST(SvgHtml, references_that_leave_the_document_are_stripped) {
  EXPECT_THAT(svg_html(R"(<use href="#a"/>)"),
              HasSubstr(R"(<use href="#a"/>)"));
  EXPECT_THAT(svg_html(R"(<image href="data:image/png;base64,AA=="/>)"),
              HasSubstr("data:image/png;base64,AA=="));

  EXPECT_THAT(svg_html(R"(<a href="javascript:evil"><rect/></a>)"),
              Not(HasSubstr("javascript")));
  EXPECT_THAT(svg_html("<a href=\"java\nscript:evil\"><rect/></a>"),
              Not(HasSubstr("evil")));
  EXPECT_THAT(svg_html(R"(<image href="https://example.com/tracker.png"/>)"),
              Not(HasSubstr("example.com")));
}

/// SMIL animates whatever attribute it is pointed at, including the ones the
/// attribute rules just removed.
TEST(SvgHtml, animating_a_stripped_attribute_is_stripped_too) {
  EXPECT_THAT(svg_html(R"(<rect><set attributeName="onclick" )"
                       R"(to="evil"/></rect>)"),
              Not(HasSubstr("evil")));
  EXPECT_THAT(svg_html(R"(<use><animate attributeName="href" )"
                       R"(values="javascript:evil"/></use>)"),
              Not(HasSubstr("javascript")));
  // an ordinary animation is left alone
  EXPECT_THAT(svg_html(R"(<rect><animate attributeName="x" to="5"/></rect>)"),
              HasSubstr(R"(attributeName="x")"));
}

TEST(SvgHtml, css_that_reaches_outside_the_document_is_stripped) {
  EXPECT_THAT(
      svg_html("<style>@import url(https://example.com/x.css);</style>"),
      Not(HasSubstr("example.com")));
  EXPECT_THAT(
      svg_html(R"(<style>rect{fill:url(https://example.com/x)}</style>)"),
      Not(HasSubstr("example.com")));
  EXPECT_THAT(svg_html("<rect style=\"fill:url(https://example.com/x)\"/>"),
              Not(HasSubstr("example.com")));

  // a fragment url is how a gradient is referenced, and stays
  EXPECT_THAT(svg_html("<style>rect{fill:url(#grad)}</style>"),
              HasSubstr("url(#grad)"));
}

/// An svg inside a document is still written as a data url — an `<img>` is
/// what the renderer has there, and it renders svg in secure static mode.
TEST(SvgHtml, an_svg_referenced_by_a_document_stays_a_data_url) {
  std::ostringstream out;
  internal::html::translate_image_src(
      File::from_memory(std::string(svg_open) + "</svg>"), out, HtmlConfig());

  EXPECT_THAT(out.str(), HasSubstr("data:image/svg+xml;base64,"));
}
