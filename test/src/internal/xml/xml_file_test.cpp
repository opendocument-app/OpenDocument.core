#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/text/text_file.hpp>
#include <odr/internal/util/xml_util.hpp>
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

std::shared_ptr<xml::XmlFile> xml_file(const std::string &content) {
  return std::make_shared<xml::XmlFile>(
      std::make_shared<text::TextFile>(File::from_memory(content).impl()));
}

/// The source view, with the css embedded as the default config asks for.
std::string xml_html(const std::string &content) {
  const HtmlService service = html::translate(DecodedFile(xml_file(content)),
                                              HtmlConfig(), Logger::null());

  std::ostringstream out;
  service.write("xml.html", out);
  return out.str();
}

std::string declared_encoding(const std::string &content) {
  std::istringstream in(content);
  return util::xml::read_declared_encoding(in);
}

} // namespace

TEST(XmlFile, an_xml_file_opens_as_xml) {
  const DecodedFile file(File::from_memory("<a><b/></a>"));

  EXPECT_EQ(file.file_type(), FileType::xml);
  EXPECT_EQ(file.file_meta().mimetype, "application/xml");
  EXPECT_TRUE(file.is_text_file());
  EXPECT_FALSE(file.is_document_file());
  EXPECT_TRUE(file.capabilities().translate_html);

  EXPECT_THAT(DecodedFile::list_file_types(File::from_memory("<a><b/></a>")),
              testing::Contains(FileType::xml));
}

/// Xml is the last resort: anything with a more specific reading keeps it.
TEST(XmlFile, an_svg_still_opens_as_an_image) {
  const DecodedFile file(
      File::from_memory(R"(<svg xmlns="http://www.w3.org/2000/svg"/>)"));

  EXPECT_EQ(file.file_type(), FileType::scalable_vector_graphics);
  EXPECT_TRUE(file.is_image_file());
}

TEST(XmlFile, malformed_xml_is_no_xml_file_and_stays_text) {
  EXPECT_THROW(std::ignore = xml_file("<a><b></a>"), NoXmlFile);

  // which is what leaves the line list in place for it
  const DecodedFile file(File::from_memory("<a><b></a>"));
  EXPECT_EQ(file.file_type(), FileType::text_file);
}

TEST(XmlFile, the_declaration_names_the_encoding) {
  // 0xe9 is `é` in latin-1 and not valid utf-8, so only the declaration can
  // get it right
  const std::string content =
      "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><a>\xe9</a>";

  EXPECT_EQ(xml_file(content)->encoding(), TextEncoding::iso_8859_1);
  EXPECT_THAT(xml_html(content), HasSubstr("é"));
}

/// An encoding we can name but not decode has no tree at all — there is no
/// handing the bytes to the browser once they are inside a parser.
TEST(XmlFile, an_encoding_we_cannot_decode_has_no_source_view) {
  const std::string content =
      "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?><a/>";

  EXPECT_THROW(std::ignore = xml_file(content), UnsupportedTextEncoding);
  EXPECT_EQ(DecodedFile(File::from_memory(content)).file_type(),
            FileType::text_file);
}

TEST(XmlDeclaration, the_encoding_pseudo_attribute_is_read_off_the_bytes) {
  EXPECT_EQ(declared_encoding(R"(<?xml version="1.0" encoding="UTF-8"?><a/>)"),
            "UTF-8");
  EXPECT_EQ(declared_encoding("<?xml version='1.0' encoding = 'latin1' ?><a/>"),
            "latin1");
  EXPECT_EQ(declared_encoding("\xef\xbb\xbf<?xml encoding=\"UTF-8\"?><a/>"),
            "UTF-8");

  EXPECT_EQ(declared_encoding(R"(<?xml version="1.0"?><a/>)"), "");
  EXPECT_EQ(declared_encoding("<a/>"), "");
  EXPECT_EQ(declared_encoding(""), "");
  // no `?>` in the probe: an unterminated declaration names nothing
  EXPECT_EQ(declared_encoding(R"(<?xml encoding="UTF-8")"), "");
}

/// Asking for the text rendering by hand gets the text rendering — the source
/// view is what an xml file translates to, not what a text file translates to.
TEST(XmlHtml, translating_it_as_a_text_file_still_writes_the_line_list) {
  const HtmlService service = html::translate(
      odr::TextFile(xml_file("<a><b/></a>")), HtmlConfig(), Logger::null());

  std::ostringstream out;
  service.write("text.html", out);
  EXPECT_THAT(out.str(), HasSubstr("odr-text-nr"));
}

/// The whole point of the view: the files anyone opens on purpose have no line
/// breaks in them.
TEST(XmlHtml, a_minified_document_is_reindented) {
  const std::string html = xml_html("<a><b><c/></b></a>");

  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-indent">  </span>)"));
  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-indent">    </span>)"));
  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-name">c</span>/&gt;)"));
}

/// `<p>a <b>x</b> b</p>` carries significant whitespace and nothing short of a
/// schema can tell it from the insignificant kind.
TEST(XmlHtml, mixed_content_is_left_alone) {
  const std::string html = xml_html("<p>a <b>x</b> b</p>");

  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-text">a </span>)"));
  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-text"> b</span>)"));
  // one line, so no fold and no indentation inside it
  EXPECT_THAT(html, Not(HasSubstr("<details")));
  EXPECT_THAT(html, Not(HasSubstr(R"(<span class="odr-xml-indent")")));
}

/// Whitespace-only text survives where it is an element's only child, which is
/// the case the parse flags are chosen for.
TEST(XmlHtml, an_elements_only_whitespace_is_content) {
  EXPECT_THAT(xml_html("<a>   </a>"),
              HasSubstr(R"(<span class="odr-xml-text">   </span>)"));
  // between two siblings it is not
  EXPECT_THAT(xml_html("<a>\n  <b/>\n</a>"),
              Not(HasSubstr(R"(<span class="odr-xml-text")")));
}

TEST(XmlHtml, everything_the_default_parse_drops_is_written) {
  const std::string html = xml_html("<?xml version=\"1.0\"?>"
                                    "<!DOCTYPE a SYSTEM \"a.dtd\">"
                                    "<?stylesheet href=\"a.xsl\"?>"
                                    "<!-- a note -->"
                                    "<a><![CDATA[x < y]]></a>");

  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-attr">version</span>)"));
  EXPECT_THAT(html, HasSubstr(R"(&lt;!DOCTYPE a SYSTEM "a.dtd"&gt;)"));
  EXPECT_THAT(html, HasSubstr(R"(&lt;?stylesheet href="a.xsl"?&gt;)"));
  EXPECT_THAT(html, HasSubstr("&lt;!-- a note --&gt;"));
  EXPECT_THAT(html, HasSubstr("&lt;![CDATA[x &lt; y]]&gt;"));
}

TEST(XmlHtml, an_element_with_element_children_folds) {
  const std::string html = xml_html("<a><b/></a>");

  EXPECT_THAT(html, HasSubstr(R"(<details class="odr-xml-node" open>)"));
  EXPECT_THAT(html, HasSubstr("<summary>"));
  // the end tag is inside the fold, so collapsing hides the whole node
  EXPECT_THAT(html, HasSubstr(R"(&lt;/<span class="odr-xml-name">a</span>)"));
}

/// An attribute value the tree gives back with a double quote in it has to be
/// delimited by the other one — the file's own quoting is not in the tree.
TEST(XmlHtml, an_attribute_value_is_quoted_so_it_needs_no_entity) {
  EXPECT_THAT(xml_html(R"(<a b='say "hi"'/>)"),
              HasSubstr(R"(<span class="odr-xml-value">'say "hi"'</span>)"));
}

/// Neither delimiter is free, so one of them has to be written as an entity.
TEST(XmlHtml, an_attribute_value_carrying_both_quotes_escapes_the_delimiter) {
  EXPECT_THAT(
      xml_html(R"(<a b="can&apos;t say &quot;hi&quot;"/>)"),
      HasSubstr(
          R"(<span class="odr-xml-value">"can't say &quot;hi&quot;"</span>)"));
}

/// The value is source text, and html would fold its spaces.
TEST(XmlHtml, an_attribute_values_whitespace_is_preserved) {
  const std::string html = xml_html(R"(<a b="x  y"/>)");

  EXPECT_THAT(html, HasSubstr(R"(<span class="odr-xml-value">"x  y"</span>)"));
  EXPECT_THAT(html, HasSubstr(".odr-xml-value,"));
}
