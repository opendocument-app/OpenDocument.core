#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

/// Three slides, ids 256 to 258, the second with a notes slide. Two sections
/// hold the first and the last two, and a custom show the second.
std::shared_ptr<internal::abstract::File>
three_slides(const std::string &source = "", const std::string &target = "",
             const bool external = false) {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>)"
      R"(<Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)"
      R"(<Override PartName="/ppt/slides/slide2.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)"
      R"(<Override PartName="/ppt/slides/slide3.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)"
      R"(<Override PartName="/ppt/notesSlides/notesSlide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml"/>)"
      R"(</Types>)");
  insert(
      zip, "_rels/.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "ppt/presentation.xml",
      R"(<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" )"
      R"(xmlns:p14="http://schemas.microsoft.com/office/powerpoint/2010/main">)"
      R"(<p:sldIdLst><p:sldId id="256" r:id="rId1"/><p:sldId id="257" r:id="rId2"/>)"
      R"(<p:sldId id="258" r:id="rId3"/></p:sldIdLst>)"
      R"(<p:custShowLst><p:custShow name="short" id="0"><p:sldLst><p:sld r:id="rId2"/>)"
      R"(</p:sldLst></p:custShow></p:custShowLst>)"
      R"(<p:extLst><p:ext uri="{521415D9-36F7-43E2-AB2F-B90AF26B5E84}"><p14:sectionLst>)"
      R"(<p14:section name="a" id="{A}"><p14:sldIdLst><p14:sldId id="256"/></p14:sldIdLst></p14:section>)"
      R"(<p14:section name="b" id="{B}"><p14:sldIdLst><p14:sldId id="257"/><p14:sldId id="258"/>)"
      R"(</p14:sldIdLst></p14:section></p14:sectionLst></p:ext></p:extLst>)"
      R"(</p:presentation>)");
  std::string relationships =
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
  for (const char *number : {"1", "2", "3"}) {
    relationships +=
        std::string(R"(<Relationship Id="rId)") + number +
        R"(" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide)" +
        number + R"(.xml"/>)";
    insert(
        zip, std::string("ppt/slides/slide") + number + ".xml",
        R"(<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main">)"
        R"(<p:cSld><p:spTree/></p:cSld></p:sld>)");
  }
  insert(zip, "ppt/_rels/presentation.xml.rels",
         relationships + "</Relationships>");
  insert(
      zip, "ppt/slides/_rels/slide2.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/notesSlide" Target="../notesSlides/notesSlide1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "ppt/notesSlides/notesSlide1.xml",
      R"(<p:notes xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"/>)");
  insert(
      zip, "ppt/notesSlides/_rels/notesSlide1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="../slides/slide2.xml"/>)"
      R"(</Relationships>)");
  if (!source.empty()) {
    insert(
        zip, source,
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target=")" +
            target + (external ? R"(" TargetMode="External)" : "") +
            R"("/></Relationships>)");
  }

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

Slide slide_at(const Document &document, const std::uint32_t index) {
  std::uint32_t i = 0;
  for (const Element slide : document.root_element().children()) {
    if (i++ == index) {
      return slide.as_slide();
    }
  }
  return {};
}

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

} // namespace

TEST(OoxmlPresentationSlides, a_removed_slide_takes_its_notes_along) {
  const Document document = decode(three_slides());

  document.remove(slide_at(document, 1));

  const Filesystem files = saved_document(document).as_filesystem();
  EXPECT_FALSE(files.is_file("/ppt/slides/slide2.xml"));
  EXPECT_FALSE(files.is_file("/ppt/slides/_rels/slide2.xml.rels"));
  EXPECT_FALSE(files.is_file("/ppt/notesSlides/notesSlide1.xml"));
  EXPECT_TRUE(files.is_file("/ppt/slides/slide3.xml"));
  const std::string types = saved_part(document, "/[Content_Types].xml");
  EXPECT_FALSE(contains(types, "slide2.xml"));
  EXPECT_FALSE(contains(types, "notesSlide1.xml"));
  EXPECT_FALSE(
      contains(saved_part(document, "/ppt/_rels/presentation.xml.rels"),
               R"(Id="rId2")"));
}

TEST(OoxmlPresentationSlides, a_removed_slide_leaves_sections_and_shows) {
  const Document document = decode(three_slides());

  document.remove(slide_at(document, 1));

  const std::string presentation =
      saved_part(document, "/ppt/presentation.xml");
  EXPECT_FALSE(contains(presentation, R"(id="257")"));
  EXPECT_FALSE(contains(presentation, R"(r:id="rId2")"));
  EXPECT_TRUE(contains(presentation, R"(<p14:sldId id="258"/>)"));
}

TEST(OoxmlPresentationSlides, incoming_references_refuse_before_removing) {
  for (const std::string &source :
       {"ppt/slides/_rels/slide1.xml.rels", "ppt/_rels/viewProps.xml.rels"}) {
    SCOPED_TRACE(source);
    const Document document =
        decode(three_slides(source, "/ppt/slides/slide2.xml"));
    const std::string before = saved_part(document, "/ppt/presentation.xml");
    const Slide slide = slide_at(document, 1);
    EXPECT_THROW(document.remove(slide), UnsupportedOperation);
    EXPECT_EQ(slide_at(document, 1), slide);
    EXPECT_EQ(saved_part(document, "/ppt/presentation.xml"), before);
    const Filesystem files = saved_document(document).as_filesystem();
    EXPECT_TRUE(files.is_file("/ppt/slides/slide2.xml"));
    EXPECT_TRUE(files.is_file("/ppt/notesSlides/notesSlide1.xml"));
  }
}

TEST(OoxmlPresentationSlides, relative_references_to_notes_also_refuse) {
  const Document document = decode(three_slides(
      "ppt/slides/_rels/slide1.xml.rels", "../notesSlides/notesSlide1.xml"));
  EXPECT_THROW(document.remove(slide_at(document, 1)), UnsupportedOperation);
}

TEST(OoxmlPresentationSlides, external_and_removed_sources_do_not_block) {
  const Document external = decode(three_slides(
      "ppt/slides/_rels/slide1.xml.rels", "/ppt/slides/slide2.xml", true));
  EXPECT_NO_THROW(external.remove(slide_at(external, 1)));

  const Document document =
      decode(three_slides("ppt/slides/_rels/slide1.xml.rels", "slide2.xml"));
  document.remove(slide_at(document, 0));
  EXPECT_NO_THROW(document.remove(slide_at(document, 0)));
  EXPECT_FALSE(saved_document(document).as_filesystem().is_file(
      "/ppt/slides/slide2.xml"));
}

TEST(OoxmlPresentationSlides, a_moved_slide_joins_the_section_it_lands_in) {
  const Document document = decode(three_slides());

  document.as_presentation().move_slide(slide_at(document, 2), 0);

  const std::string presentation =
      saved_part(document, "/ppt/presentation.xml");
  EXPECT_TRUE(contains(
      presentation,
      R"(<p:sldIdLst><p:sldId id="258" r:id="rId3"/><p:sldId id="256" r:id="rId1"/>)"));
  EXPECT_TRUE(contains(
      presentation,
      R"(<p14:sldIdLst><p14:sldId id="258"/><p14:sldId id="256"/></p14:sldIdLst>)"));
  EXPECT_TRUE(contains(
      presentation, R"(<p14:sldIdLst><p14:sldId id="257"/></p14:sldIdLst>)"));
}

TEST(OoxmlPresentationSlides, a_slide_moved_after_another_follows_its_section) {
  const Document document = decode(three_slides());

  document.as_presentation().move_slide(slide_at(document, 0), 1);

  const std::string presentation =
      saved_part(document, "/ppt/presentation.xml");
  EXPECT_TRUE(contains(presentation, R"(<p14:sldIdLst></p14:sldIdLst>)") ||
              contains(presentation, R"(<p14:sldIdLst/>)"));
  EXPECT_TRUE(contains(
      presentation,
      R"(<p14:sldId id="257"/><p14:sldId id="256"/><p14:sldId id="258"/>)"));
}
