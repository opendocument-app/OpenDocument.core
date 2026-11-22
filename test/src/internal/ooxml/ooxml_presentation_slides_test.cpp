#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/filesystem.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

struct IncomingReference {
  std::string source{};
  std::string target{};
  bool external{false};
  std::string slide{};
  std::string view{};
  std::string extra_relationships{};
  std::string other_part{};
};

/// Three slides, ids 256 to 258, the second with a notes slide. Two sections
/// hold the first and the last two, and a custom show the second.
std::shared_ptr<internal::abstract::File>
three_slides(const IncomingReference &reference = {}) {
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
        R"(<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" )"
        R"(xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" )"
        R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<p:cSld><p:spTree>)" +
            (std::string_view(number) == "1" ? reference.slide : "") +
            R"(</p:spTree></p:cSld></p:sld>)");
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
  if (!reference.source.empty()) {
    insert(
        zip, reference.source,
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target=")" +
            reference.target +
            (reference.external ? R"(" TargetMode="External)" : "") + R"("/>)" +
            reference.extra_relationships + R"(</Relationships>)");
  }
  if (reference.source == "ppt/_rels/viewProps.xml.rels") {
    insert(
        zip, "ppt/viewProps.xml",
        R"(<p:viewPr xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" )"
        R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><p:outlineViewPr><p:sldLst>)" +
            reference.view + R"(</p:sldLst></p:outlineViewPr></p:viewPr>)");
  }

  if (!reference.other_part.empty()) {
    insert(zip, "ppt/unknown.xml", reference.other_part);
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

Text first_text(const Element &element) {
  if (const Text text = element.as_text()) {
    return text;
  }
  for (const Element child : element.children()) {
    if (const Text text = first_text(child)) {
      return text;
    }
  }
  return {};
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

TEST(OoxmlPresentationSlides, unknown_references_remain_after_removing) {
  for (const std::string source :
       {"ppt/slides/_rels/slide1.xml.rels", "ppt/_rels/viewProps.xml.rels"}) {
    SCOPED_TRACE(source);
    const std::string unknown =
        R"(<a:hlinkClick xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" r:id="rId1"/>)"
        R"(<p:ext xmlns:rel="http://schemas.openxmlformats.org/officeDocument/2006/relationships" rel:id="rId1"/>)";
    const Document document =
        decode(three_slides({.source = source,
                             .target = "/ppt/slides/slide2.xml",
                             .slide = unknown,
                             .view = unknown}));
    const std::string relations_before = saved_part(document, "/" + source);
    document.remove(slide_at(document, 1));
    const std::string part =
        saved_part(document, source == "ppt/_rels/viewProps.xml.rels"
                                 ? "/ppt/viewProps.xml"
                                 : "/ppt/slides/slide1.xml");
    EXPECT_TRUE(contains(part, R"(rel:id="rId1")"));
    EXPECT_FALSE(contains(part, "hlinkClick"));
    EXPECT_EQ(saved_part(document, "/" + source), relations_before);
    const Filesystem files = saved_document(document).as_filesystem();
    EXPECT_FALSE(files.is_file("/ppt/slides/slide2.xml"));
    EXPECT_FALSE(files.is_file("/ppt/notesSlides/notesSlide1.xml"));
  }
}

TEST(OoxmlPresentationSlides, unknown_notes_references_do_not_block_removal) {
  const Document document =
      decode(three_slides({.source = "ppt/slides/_rels/slide1.xml.rels",
                           .target = "../notesSlides/notesSlide1.xml",
                           .slide = R"(<p:ext r:id="rId1"/>)"}));
  const std::string before = saved_part(document, "/ppt/slides/slide1.xml");
  document.remove(slide_at(document, 1));
  EXPECT_EQ(saved_part(document, "/ppt/slides/slide1.xml"), before);
  EXPECT_TRUE(
      contains(saved_part(document, "/ppt/slides/_rels/slide1.xml.rels"),
               "../notesSlides/notesSlide1.xml"));
  EXPECT_FALSE(saved_document(document).as_filesystem().is_file(
      "/ppt/notesSlides/notesSlide1.xml"));
}

TEST(OoxmlPresentationSlides, external_and_removed_sources_do_not_block) {
  const Document external =
      decode(three_slides({.source = "ppt/slides/_rels/slide1.xml.rels",
                           .target = "/ppt/slides/slide2.xml",
                           .external = true}));
  EXPECT_NO_THROW(external.remove(slide_at(external, 1)));

  const Document document = decode(three_slides(
      {.source = "ppt/slides/_rels/slide1.xml.rels", .target = "slide2.xml"}));
  document.remove(slide_at(document, 0));
  EXPECT_NO_THROW(document.remove(slide_at(document, 0)));
  EXPECT_FALSE(saved_document(document).as_filesystem().is_file(
      "/ppt/slides/slide2.xml"));
}

TEST(OoxmlPresentationSlides, removal_unlinks_text_shapes_and_hover_actions) {
  const Document document = decode(three_slides(
      {.source = "ppt/slides/_rels/slide1.xml.rels",
       .target = "slide2.xml",
       .slide =
           R"(<p:sp><p:nvSpPr><p:cNvPr id="1" name="Button"><a:hlinkClick r:id="rId1" action="ppaction://hlinksldjump"/></p:cNvPr><p:cNvSpPr/><p:nvPr/></p:nvSpPr><p:spPr/>)"
           R"(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr b="1"><a:hlinkClick r:id="rId1"/><a:hlinkMouseOver xmlns:rel="http://schemas.openxmlformats.org/officeDocument/2006/relationships" rel:id="rId1"/></a:rPr><a:t>Keep this text</a:t></a:r>)"
           R"(<a:r><a:rPr><a:hlinkClick r:id="web"/></a:rPr><a:t>External</a:t></a:r>)"
           R"(<a:r><a:rPr><a:hlinkClick r:id="last"/></a:rPr><a:t>Last slide</a:t></a:r></a:p></p:txBody></p:sp>)",
       .extra_relationships =
           R"(<Relationship Id="web" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink" Target="https://example.com" TargetMode="External"/>)"
           R"(<Relationship Id="last" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="/ppt/slides/slide3.xml"/>)"}));
  const Slide first = slide_at(document, 0);
  const Element frame = first.first_child();
  const Text text = first_text(first);
  ASSERT_TRUE(text);
  text.set_content("Keep this text, edited before");
  document.remove(slide_at(document, 1));
  EXPECT_EQ(first.first_child(), frame);
  EXPECT_EQ(text.content(), "Keep this text, edited before");
  text.set_content(text.content() + ", and after");
  const std::string slide = saved_part(document, "/ppt/slides/slide1.xml");
  EXPECT_TRUE(contains(slide, "Keep this text, edited before, and after"));
  EXPECT_TRUE(contains(slide, R"(name="Button")"));
  EXPECT_TRUE(contains(slide, R"(b="1")"));
  EXPECT_FALSE(contains(slide, R"(id="rId1")"));
  EXPECT_FALSE(contains(slide, "hlinkMouseOver"));
  EXPECT_FALSE(contains(slide, "hlinksldjump"));
  EXPECT_TRUE(contains(slide, R"(r:id="web")"));
  EXPECT_TRUE(contains(slide, R"(r:id="last")"));
  EXPECT_FALSE(contains(
      saved_part(document, "/ppt/slides/_rels/slide1.xml.rels"), "slide2.xml"));

  const Document reopened = saved_document(document);
  reopened.remove(slide_at(reopened, 1));
  const std::string after = saved_part(reopened, "/ppt/slides/slide1.xml");
  EXPECT_TRUE(contains(after, "Last slide"));
  EXPECT_FALSE(contains(after, R"(r:id="last")"));
  EXPECT_TRUE(contains(after, R"(r:id="web")"));
  EXPECT_TRUE(
      contains(saved_part(reopened, "/ppt/slides/_rels/slide1.xml.rels"),
               "https://example.com"));
}

TEST(OoxmlPresentationSlides,
     removal_cleans_outline_entries_and_cached_relationships) {
  const Document document = decode(three_slides(
      {.source = "ppt/_rels/viewProps.xml.rels",
       .target = "/ppt/slides/slide2.xml",
       .view =
           R"(<p:sld r:id="rId1" collapse="1"/><p:sld r:id="last" collapse="0"/><p:sld r:id="rId1"/>)",
       .extra_relationships =
           R"(<Relationship Id="last" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide3.xml"/>)"}));
  document.remove(slide_at(document, 1));
  const std::string view = saved_part(document, "/ppt/viewProps.xml");
  EXPECT_FALSE(contains(view, R"(r:id="rId1")"));
  EXPECT_TRUE(contains(view, R"(r:id="last" collapse="0")"));
  EXPECT_FALSE(contains(saved_part(document, "/ppt/_rels/viewProps.xml.rels"),
                        "slide2.xml"));
  document.remove(slide_at(document, 1));
  EXPECT_FALSE(
      contains(saved_part(saved_document(document), "/ppt/viewProps.xml"),
               R"(r:id="last")"));
  EXPECT_FALSE(contains(saved_part(document, "/ppt/_rels/viewProps.xml.rels"),
                        "slide3.xml"));
}

TEST(OoxmlPresentationSlides, an_unused_slide_relationship_is_removed) {
  const Document document = decode(three_slides(
      {.source = "ppt/slides/_rels/slide1.xml.rels", .target = "slide2.xml"}));
  document.remove(slide_at(document, 1));
  EXPECT_FALSE(contains(
      saved_part(document, "/ppt/slides/_rels/slide1.xml.rels"), "slide2.xml"));
}

TEST(OoxmlPresentationSlides, unknown_relationship_types_do_not_block_cleanup) {
  const Document document = decode(three_slides(
      {.source = "ppt/slides/_rels/slide1.xml.rels",
       .target = "slide2.xml",
       .slide = R"(<a:hlinkClick r:id="rId1"/>)",
       .extra_relationships =
           R"(<Relationship Id="custom" Type="urn:custom:slide" Target="slide2.xml"/>)"}));
  document.remove(slide_at(document, 1));
  EXPECT_FALSE(
      contains(saved_part(document, "/ppt/slides/slide1.xml"), "hlinkClick"));
  const std::string relations =
      saved_part(document, "/ppt/slides/_rels/slide1.xml.rels");
  EXPECT_FALSE(contains(relations, R"(Id="rId1")"));
  EXPECT_TRUE(
      contains(relations, R"(Type="urn:custom:slide" Target="slide2.xml")"));
  EXPECT_FALSE(saved_document(document).as_filesystem().is_file(
      "/ppt/slides/slide2.xml"));
}

TEST(OoxmlPresentationSlides,
     unknown_or_unreadable_parts_do_not_block_removal) {
  for (const std::string &content :
       {std::string(), std::string("<broken"), std::string("<custom />")}) {
    SCOPED_TRACE(content);
    const Document document =
        decode(three_slides({.source = "ppt/_rels/unknown.xml.rels",
                             .target = "slides/slide2.xml",
                             .other_part = content}));
    const std::string relations =
        saved_part(document, "/ppt/_rels/unknown.xml.rels");
    document.remove(slide_at(document, 1));
    EXPECT_EQ(saved_part(document, "/ppt/_rels/unknown.xml.rels"), relations);
    if (!content.empty()) {
      EXPECT_EQ(saved_part(document, "/ppt/unknown.xml"), content);
    }
    EXPECT_FALSE(saved_document(document).as_filesystem().is_file(
        "/ppt/slides/slide2.xml"));
  }
}

TEST(OoxmlPresentationSlides, unreadable_relationship_parts_are_left_alone) {
  const Document document =
      decode(three_slides({.source = "ppt/_rels/unknown.xml.rels",
                           .target = "slides/slide2.xml",
                           .extra_relationships = "<broken"}));
  const std::string before =
      saved_part(document, "/ppt/_rels/unknown.xml.rels");
  document.remove(slide_at(document, 1));
  EXPECT_EQ(saved_part(document, "/ppt/_rels/unknown.xml.rels"), before);
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
