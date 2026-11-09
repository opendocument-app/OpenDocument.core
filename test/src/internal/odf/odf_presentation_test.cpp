#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>

using namespace odr;

namespace {

std::string render(const std::string &source) {
  const HtmlConfig config(
      (std::filesystem::current_path() / "odf_presentation").string());
  std::ostringstream out;
  html::translate(open(File::from_memory(source)), config)
      .list_views()
      .at(0)
      .write_html(out);
  return out.str();
}

std::size_t count(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + needle.size())) {
    ++result;
  }
  return result;
}

/// A presentation whose master page holds a title and an outline placeholder,
/// as PowerPoint's and Impress's layouts do, beside a logo that is no
/// placeholder.
std::string presentation_with_placeholders() {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.presentation">)"
         R"(<office:master-styles><style:master-page style:name="Master">)"
         R"(<draw:frame presentation:class="title" svg:x="1cm" svg:y="1cm")"
         R"( svg:width="20cm" svg:height="2cm"><draw:text-box>)"
         R"(<text:p>Click to edit Master title style</text:p>)"
         R"(</draw:text-box></draw:frame>)"
         R"(<draw:frame presentation:class="outline" svg:x="1cm" svg:y="4cm")"
         R"( svg:width="20cm" svg:height="10cm"><draw:text-box>)"
         R"(<text:p>Second level</text:p></draw:text-box></draw:frame>)"
         R"(<draw:frame svg:x="22cm" svg:y="1cm" svg:width="3cm")"
         R"( svg:height="1cm"><draw:text-box><text:p>Logo</text:p>)"
         R"(</draw:text-box></draw:frame>)"
         R"(</style:master-page></office:master-styles>)"
         R"(<office:body><office:presentation><draw:page draw:name="one")"
         R"( draw:master-page-name="Master">)"
         R"(<draw:frame presentation:class="title" svg:x="1cm" svg:y="1cm")"
         R"( svg:width="20cm" svg:height="2cm"><draw:text-box>)"
         R"(<text:p>Quarterly report</text:p></draw:text-box></draw:frame>)"
         R"(</draw:page></office:presentation></office:body>)"
         R"(</office:document>)";
}

} // namespace

TEST(OdfPresentation, a_master_page_keeps_no_placeholder) {
  const Document document =
      open(File::from_memory(presentation_with_placeholders()))
          .as_document_file()
          .document();
  const Slide slide = document.root_element().first_child().as_slide();
  ASSERT_TRUE(slide);

  std::size_t children = 0;
  for (const Element child : slide.master_page().children()) {
    EXPECT_EQ(child.type(), ElementType::frame);
    ++children;
  }
  EXPECT_EQ(children, 1U);
}

TEST(OdfPresentation, a_slide_draws_its_text_not_the_master_prompts) {
  const std::string html = render(presentation_with_placeholders());

  EXPECT_EQ(count(html, "Quarterly report"), 1U);
  EXPECT_EQ(count(html, "Logo"), 1U);
  EXPECT_EQ(count(html, "Click to edit"), 0U);
  EXPECT_EQ(count(html, "Second level"), 0U);
}
