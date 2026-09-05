#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;

namespace {

/// A flat document parses without namespace declarations - prefixes are part
/// of the tag name and nothing resolves them.
std::string flat_document(const std::string &mimetype, const std::string &body,
                          const std::string &styles = "") {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")" +
         mimetype + R"(">)" + styles + "<office:body>" + body +
         "</office:body></office:document>";
}

std::string flat_text(const std::string &body, const std::string &styles = "") {
  return flat_document("application/vnd.oasis.opendocument.text",
                       "<office:text>" + body + "</office:text>", styles);
}

Element first_of_type(const Element root, const ElementType type) {
  for (const Element child : root.children()) {
    if (child.type() == type) {
      return child;
    }
    if (const Element match = first_of_type(child, type)) {
      return match;
    }
  }
  return {};
}

// 1x1 png
constexpr const char *png_base64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGA"
    "hKmMIQAAAABJRU5ErkJggg==";

/// A packaged odt holding @p body, written to @p name in the working directory.
std::string packaged_text(const std::string &name, const std::string &body) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document-content><office:body><office:text>)" +
      body + R"(</office:text></office:body></office:document-content>)";

  odr::internal::zip::ZipArchive zip;
  zip.insert_file(std::end(zip), odr::internal::RelPath("mimetype"),
                  std::make_shared<odr::internal::MemoryFile>(
                      "application/vnd.oasis.opendocument.text"),
                  0);
  zip.insert_file(std::end(zip), odr::internal::RelPath("content.xml"),
                  std::make_shared<odr::internal::MemoryFile>(content));

  const std::string path = (std::filesystem::current_path() / name).string();
  std::ofstream out(path, std::ios::binary);
  zip.save(out);
  return path;
}

} // namespace

TEST(FlatOpenDocumentFile, the_root_mimetype_names_the_document_type) {
  const struct {
    const char *mimetype;
    FileType file_type;
    DocumentType document_type;
  } cases[]{
      {"application/vnd.oasis.opendocument.text", FileType::opendocument_text,
       DocumentType::text},
      {"application/vnd.oasis.opendocument.presentation",
       FileType::opendocument_presentation, DocumentType::presentation},
      {"application/vnd.oasis.opendocument.spreadsheet",
       FileType::opendocument_spreadsheet, DocumentType::spreadsheet},
      {"application/vnd.oasis.opendocument.graphics",
       FileType::opendocument_graphics, DocumentType::drawing},
  };

  for (const auto &[mimetype, file_type, document_type] : cases) {
    const DecodedFile file(
        File::from_memory(flat_document(mimetype, "<office:text/>")));

    EXPECT_EQ(file.file_type(), file_type);
    EXPECT_EQ(file.file_category(), FileCategory::document);
    EXPECT_TRUE(file.is_document_file());
    EXPECT_EQ(file.as_document_file().document_type(), document_type);
  }
}

/// The `-flat-xml` mimetypes are what a caller names the file, not what the
/// root carries.
TEST(FlatOpenDocumentFile, the_flat_mimetype_is_read_too_and_reported_back) {
  const DecodedFile file(File::from_memory(
      flat_document("application/vnd.oasis.opendocument.spreadsheet-flat-xml",
                    "<office:spreadsheet/>")));

  EXPECT_EQ(file.file_type(), FileType::opendocument_spreadsheet);
  EXPECT_EQ(file.file_meta().mimetype,
            "application/vnd.oasis.opendocument.spreadsheet-flat-xml");
}

TEST(FlatOpenDocumentFile, other_xml_is_left_to_the_source_view) {
  EXPECT_EQ(DecodedFile(File::from_memory("<office:document/>")).file_type(),
            FileType::xml);
  EXPECT_EQ(
      DecodedFile(File::from_memory(
                      R"(<office:document office:mimetype="text/plain"/>)"))
          .file_type(),
      FileType::xml);
  EXPECT_EQ(DecodedFile(File::from_memory("<a/>")).file_type(), FileType::xml);
}

TEST(FlatOpenDocumentFile, opening_it_as_a_document_file_works) {
  const DocumentFile file =
      DocumentFile::from_memory(flat_text("<text:p>Hello</text:p>"));

  EXPECT_EQ(file.file_type(), FileType::opendocument_text);
  EXPECT_FALSE(file.password_encrypted());
}

TEST(FlatOpenDocumentFile, opening_it_as_a_named_type_works) {
  const std::string source = flat_text("<text:p>Hello</text:p>");

  EXPECT_EQ(DecodedFile(File::from_memory(source), FileType::opendocument_text)
                .file_type(),
            FileType::opendocument_text);
  EXPECT_THROW(std::ignore = DecodedFile(File::from_memory(source),
                                         FileType::opendocument_graphics),
               UnknownFileType);
}

TEST(FlatOpenDocumentFile, the_body_decodes_to_the_same_tree_as_a_package) {
  const Document document =
      DocumentFile::from_memory(
          flat_text("<text:p>Hello <text:span>flat</text:span></text:p>"))
          .document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  const Element paragraph =
      first_of_type(document.root_element(), ElementType::paragraph);
  ASSERT_TRUE(paragraph);
  EXPECT_EQ(paragraph.first_child().as_text().content(), "Hello ");
}

/// A package splits automatic and named styles over two files; a flat document
/// has both under its one root.
TEST(FlatOpenDocumentFile, styles_resolve_from_the_single_root) {
  const Document document =
      DocumentFile::from_memory(
          flat_text(
              R"(<text:p text:style-name="P1">Hello</text:p>)",
              R"(<office:styles>)"
              R"(<style:style style:name="Base" style:family="paragraph">)"
              R"(<style:text-properties fo:font-weight="bold"/>)"
              R"(</style:style>)"
              R"(</office:styles>)"
              R"(<office:automatic-styles>)"
              R"(<style:style style:name="P1" style:family="paragraph")"
              R"( style:parent-style-name="Base">)"
              R"(<style:text-properties fo:font-size="24pt"/>)"
              R"(</style:style>)"
              R"(</office:automatic-styles>)"))
          .document();

  const Element paragraph =
      first_of_type(document.root_element(), ElementType::paragraph);
  ASSERT_TRUE(paragraph);

  const TextStyle style = paragraph.as_paragraph().text_style();
  EXPECT_EQ(style.font_size, Measure("24pt"));
  EXPECT_EQ(style.font_weight, FontWeight::bold);
}

namespace {

/// One paragraph per entry, the second carrying @p style_name.
std::string three_paragraphs(const std::string &style_name,
                             const std::string &properties) {
  return flat_text(R"(<text:p>one</text:p>)"
                   R"(<text:p text:style-name=")" +
                       style_name +
                       R"(">two</text:p>)"
                       R"(<text:p>three</text:p>)",
                   R"(<office:automatic-styles>)"
                   R"(<style:style style:name=")" +
                       style_name +
                       R"(" style:family="paragraph">)"
                       R"(<style:paragraph-properties )" +
                       properties +
                       R"(/></style:style>)"
                       R"(</office:automatic-styles>)");
}

/// The rendered document, with the page box the margin config turns on.
std::string render(const std::string &source) {
  HtmlConfig config((std::filesystem::current_path() / "flat_break").string());
  config.text_document_margin = true;

  std::ostringstream out;
  html::translate(DecodedFile(File::from_memory(source)), config)
      .list_views()
      .at(0)
      .write_html(out);
  return out.str();
}

/// `break_before` of every top-level paragraph, in document order.
std::vector<std::optional<BreakType>>
breaks_before_of(const std::string &source) {
  const Document document = DocumentFile::from_memory(source).document();

  std::vector<std::optional<BreakType>> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_paragraph().style().break_before);
  }
  return result;
}

std::size_t count(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + needle.size())) {
    ++result;
  }
  return result;
}

/// Told apart from the stylesheet's own mentions of the class.
std::size_t page_boxes(const std::string &html) {
  return count(html, R"(class="odr-page-outer")");
}

} // namespace

/// [OpenDocument] 20.86; not the `text:soft-page-break` element.
TEST(FlatOpenDocumentFile,
     a_manual_page_break_is_read_off_the_paragraph_style) {
  EXPECT_EQ(
      std::vector<std::optional<BreakType>>(
          {std::nullopt, BreakType::page, std::nullopt}),
      breaks_before_of(three_paragraphs("P1", R"(fo:break-before="page")")));
}

/// `auto` clears an inherited break; unset says nothing.
TEST(FlatOpenDocumentFile, an_automatic_break_reads_as_none_rather_than_unset) {
  EXPECT_EQ(
      std::vector<std::optional<BreakType>>(
          {std::nullopt, BreakType::none, std::nullopt}),
      breaks_before_of(three_paragraphs("P1", R"(fo:break-before="auto")")));
}

/// And states itself in css for paged media.
TEST(FlatOpenDocumentFile, a_manual_page_break_splits_the_page_box) {
  const std::string html =
      render(three_paragraphs("P1", R"(fo:break-before="page")"));

  EXPECT_EQ(2U, page_boxes(html));
  EXPECT_EQ(1U, count(html, "break-before:page"));
}

/// It would otherwise open a sheet the document does not have.
TEST(FlatOpenDocumentFile, a_break_on_the_first_block_leaves_no_empty_sheet) {
  const std::string source =
      flat_text(R"(<text:p text:style-name="P1">one</text:p>)"
                R"(<text:p>two</text:p>)",
                R"(<office:automatic-styles>)"
                R"(<style:style style:name="P1" style:family="paragraph">)"
                R"(<style:paragraph-properties fo:break-before="page"/>)"
                R"(</style:style></office:automatic-styles>)");

  EXPECT_EQ(1U, page_boxes(render(source)));
}

/// [OpenDocument] 5.1.1: the producer's own layout, so not an element of ours.
TEST(FlatOpenDocumentFile, a_soft_page_break_is_not_content) {
  const std::string source =
      flat_text(R"(<text:p>one<text:soft-page-break/>two</text:p>)");

  const Document document = DocumentFile::from_memory(source).document();
  const Element paragraph =
      first_of_type(document.root_element(), ElementType::paragraph);
  ASSERT_TRUE(paragraph);
  for (const Element child : paragraph.children()) {
    EXPECT_EQ(ElementType::text, child.type());
  }

  EXPECT_EQ(1U, page_boxes(render(source)));
}

/// Without the page box there is no sheet to split.
TEST(FlatOpenDocumentFile, a_reflowed_document_states_the_break_in_css_only) {
  HtmlConfig config((std::filesystem::current_path() / "flat_reflow").string());
  config.text_document_margin = false;

  std::ostringstream out;
  html::translate(DecodedFile(File::from_memory(
                      three_paragraphs("P1", R"(fo:break-before="page")"))),
                  config)
      .list_views()
      .at(0)
      .write_html(out);

  EXPECT_EQ(0U, page_boxes(out.str()));
  EXPECT_EQ(1U, count(out.str(), "break-before:page"));
}

/// Without a package there is nowhere to put an image but the markup.
TEST(FlatOpenDocumentFile, an_embedded_image_is_internal_and_decodes) {
  const Document document =
      DocumentFile::from_memory(
          flat_text(std::string("<text:p><draw:frame><draw:image>"
                                "<office:binary-data>") +
                    png_base64 +
                    "</office:binary-data></draw:image></draw:frame></text:p>"))
          .document();

  const Element element =
      first_of_type(document.root_element(), ElementType::image);
  ASSERT_TRUE(element);

  const Image image = element.as_image();
  EXPECT_TRUE(image.is_internal());
  EXPECT_FALSE(image.href().empty());

  const std::optional<File> file = image.file();
  ASSERT_TRUE(file.has_value());
  EXPECT_EQ(DecodedFile(*file).file_type(),
            FileType::portable_network_graphics);
}

/// A flat document has no package, so a linked image stays a plain link.
TEST(FlatOpenDocumentFile, a_linked_image_is_not_internal) {
  const Document document =
      DocumentFile::from_memory(
          flat_text(R"(<text:p><draw:frame><draw:image )"
                    R"(xlink:href="https://example.org/a.png"/>)"
                    R"(</draw:frame></text:p>)"))
          .document();

  const Element element =
      first_of_type(document.root_element(), ElementType::image);
  ASSERT_TRUE(element);

  const Image image = element.as_image();
  EXPECT_FALSE(image.is_internal());
  EXPECT_EQ(image.href(), "https://example.org/a.png");
}

TEST(FlatOpenDocumentFile, the_statistics_give_the_entry_count) {
  const auto meta = [](const std::string &statistic) {
    return "<office:meta><meta:document-statistic " + statistic +
           "/></office:meta>";
  };

  const DecodedFile text(File::from_memory(
      flat_document("application/vnd.oasis.opendocument.text", "<office:text/>",
                    meta(R"(meta:page-count="7")"))));
  EXPECT_EQ(text.file_meta().entry_count, 7);

  const DecodedFile spreadsheet(File::from_memory(
      flat_document("application/vnd.oasis.opendocument.spreadsheet",
                    "<office:spreadsheet/>", meta(R"(meta:table-count="3")"))));
  EXPECT_EQ(spreadsheet.file_meta().entry_count, 3);

  // the statistic a text document does not count
  const DecodedFile mismatched(File::from_memory(
      flat_document("application/vnd.oasis.opendocument.text", "<office:text/>",
                    meta(R"(meta:table-count="3")"))));
  EXPECT_FALSE(mismatched.file_meta().entry_count.has_value());
}

/// A flat document is well formed xml too, so both readings are reported.
TEST(FlatOpenDocumentFile, it_is_listed_next_to_the_source_view) {
  const std::vector<FileType> types = DecodedFile::list_file_types(
      File::from_memory(flat_text("<text:p>Hello</text:p>")));

  EXPECT_NE(std::ranges::find(types, FileType::xml), std::end(types));
  EXPECT_NE(std::ranges::find(types, FileType::opendocument_text),
            std::end(types));
}

/// There is no package behind a flat document; asking for one answers empty.
TEST(FlatOpenDocumentFile, it_has_an_empty_filesystem) {
  const Document document =
      DocumentFile::from_memory(flat_text("<text:p>Hello</text:p>")).document();

  const Filesystem filesystem = document.as_filesystem();
  EXPECT_FALSE(filesystem.exists("/content.xml"));
  EXPECT_TRUE(filesystem.file_walker("/").end());
}

/// The renderer names the resource it writes after the href.
TEST(FlatOpenDocumentFile, embedded_images_get_distinct_hrefs) {
  const std::string image = std::string("<draw:frame><draw:image>"
                                        "<office:binary-data>") +
                            png_base64 +
                            "</office:binary-data></draw:image></draw:frame>";
  const Document document =
      DocumentFile::from_memory(
          flat_text("<text:p>" + image + image + "</text:p>"))
          .document();

  std::vector<std::string> hrefs;
  for (const Element child :
       first_of_type(document.root_element(), ElementType::paragraph)
           .children()) {
    if (const Element image_element =
            first_of_type(child, ElementType::image)) {
      hrefs.push_back(image_element.as_image().href());
    }
  }

  ASSERT_EQ(hrefs.size(), 2);
  EXPECT_NE(hrefs[0], hrefs[1]);
}

/// An `xlink:href` beside the bytes names no file of ours and must not reach
/// the renderer as a path.
TEST(FlatOpenDocumentFile,
     an_embedded_image_does_not_take_its_href_from_the_markup) {
  const Document document =
      DocumentFile::from_memory(
          flat_text(std::string(R"(<text:p><draw:frame>)")
                        .append(R"(<draw:image xlink:href="../../evil.html">)")
                        .append("<office:binary-data>")
                        .append(png_base64)
                        .append("</office:binary-data></draw:image>")
                        .append("</draw:frame></text:p>")))
          .document();

  const Element element =
      first_of_type(document.root_element(), ElementType::image);
  ASSERT_TRUE(element);

  const Image image = element.as_image();
  EXPECT_TRUE(image.is_internal());
  EXPECT_EQ(image.href().find(".."), std::string::npos);
  EXPECT_TRUE(internal::Path(image.href()).relative());
}

/// `office:binary-data` is legal in a package too, and is read there now.
TEST(FlatOpenDocumentFile, a_packaged_embedded_image_decodes_as_well) {
  const std::string path = packaged_text(
      "packaged_binary_data.odt",
      std::string("<text:p><draw:frame><draw:image><office:binary-data>")
          .append(png_base64)
          .append("</office:binary-data></draw:image></draw:frame></text:p>"));

  const Document document = DocumentFile(path).document();

  const Element element =
      first_of_type(document.root_element(), ElementType::image);
  ASSERT_TRUE(element);

  const Image image = element.as_image();
  EXPECT_TRUE(image.is_internal());

  const std::optional<File> file = image.file();
  ASSERT_TRUE(file.has_value());
  EXPECT_EQ(DecodedFile(*file).file_type(),
            FileType::portable_network_graphics);
}

TEST(FlatOpenDocumentFile, packaged_markup_bytes_beat_the_href) {
  const std::string path = packaged_text(
      "packaged_binary_data_and_href.odt",
      std::string(R"(<text:p><draw:frame>)")
          .append(R"(<draw:image xlink:href="Pictures/absent.png">)")
          .append("<office:binary-data>")
          .append(png_base64)
          .append("</office:binary-data></draw:image>")
          .append("</draw:frame></text:p>"));

  // the element holds a bare pointer into the document, so the document has
  // to outlive it
  const Document document = DocumentFile(path).document();

  const Element element =
      first_of_type(document.root_element(), ElementType::image);
  ASSERT_TRUE(element);

  const Image image = element.as_image();
  EXPECT_TRUE(image.is_internal());
  EXPECT_NE(image.href(), "Pictures/absent.png");
  ASSERT_TRUE(image.file().has_value());
}

TEST(FlatOpenDocumentFile, it_renders_its_embedded_image_embedded_or_linked) {
  const std::string source =
      flat_text(std::string("<text:p><draw:frame><draw:image>"
                            "<office:binary-data>")
                    .append(png_base64)
                    .append("</office:binary-data></draw:image>"
                            "</draw:frame></text:p>"));

  {
    HtmlConfig config(
        (std::filesystem::current_path() / "flat_embed").string());
    config.embed_images = true;

    std::ostringstream out;
    html::translate(DecodedFile(File::from_memory(source)), config)
        .list_views()
        .at(0)
        .write_html(out);

    EXPECT_NE(out.str().find("data:image/png;base64,"), std::string::npos);
  }

  {
    HtmlConfig config((std::filesystem::current_path() / "flat_link").string());
    config.embed_images = false;

    const HtmlService service =
        html::translate(DecodedFile(File::from_memory(source)), config);

    std::ostringstream out;
    const HtmlResources resources = service.list_views().at(0).write_html(out);

    std::size_t images = 0;
    for (const auto &[resource, location] : resources) {
      if (resource.type() != HtmlResourceType::image ||
          !resource.is_accessible()) {
        continue;
      }
      ++images;

      ASSERT_TRUE(location.has_value()) << resource.name();
      EXPECT_TRUE(internal::Path(*location).relative()) << *location;
      EXPECT_TRUE(service.exists(*location)) << *location;

      std::ostringstream served;
      service.write(*location, served);
      EXPECT_FALSE(served.str().empty()) << *location;
    }
    EXPECT_EQ(images, 1);
  }
}

namespace {

/// `direction` of every top-level paragraph, in document order.
std::vector<std::optional<TextDirection>>
directions_of(const std::string &source) {
  const Document document = DocumentFile::from_memory(source).document();

  std::vector<std::optional<TextDirection>> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_paragraph().style().direction);
  }
  return result;
}

} // namespace

/// [OpenDocument] 20.404.
TEST(FlatOpenDocumentFile, a_right_to_left_writing_mode_reads_as_a_direction) {
  EXPECT_EQ(
      std::vector<std::optional<TextDirection>>(
          {std::nullopt, TextDirection::right_to_left, std::nullopt}),
      directions_of(three_paragraphs("P1", R"(style:writing-mode="rl-tb")")));
}

/// Neither is a left-to-right claim.
TEST(FlatOpenDocumentFile, a_writing_mode_without_a_side_names_no_direction) {
  for (const char *mode : {"tb-rl", "tb-lr", "tb", "page"}) {
    EXPECT_EQ(std::vector<std::optional<TextDirection>>(
                  {std::nullopt, std::nullopt, std::nullopt}),
              directions_of(three_paragraphs(
                  "P1", R"(style:writing-mode=")" + std::string(mode) + "\"")))
        << mode;
  }
}

/// The default paragraph style, reached through the family fallback.
TEST(FlatOpenDocumentFile,
     the_default_style_carries_direction_down_the_family_chain) {
  const std::string source = flat_text(
      R"(<text:p text:style-name="Standard">one</text:p>)"
      R"(<text:p text:style-name="P1">two</text:p>)",
      R"(<office:styles>)"
      R"(<style:default-style style:family="paragraph">)"
      R"(<style:paragraph-properties style:writing-mode="rl-tb"/>)"
      R"(</style:default-style>)"
      R"(<style:style style:name="Standard" style:family="paragraph"/>)"
      R"(</office:styles>)"
      R"(<office:automatic-styles>)"
      R"(<style:style style:name="P1" style:family="paragraph")"
      R"( style:parent-style-name="Standard"/>)"
      R"(</office:automatic-styles>)");

  EXPECT_EQ(std::vector<std::optional<TextDirection>>(
                {TextDirection::right_to_left, TextDirection::right_to_left}),
            directions_of(source));
}

/// The root says it once; a paragraph repeating it stays silent.
TEST(FlatOpenDocumentFile, a_direction_reaches_the_css_only_where_it_differs) {
  const std::string rtl =
      render(three_paragraphs("P1", R"(style:writing-mode="rl-tb")"));
  EXPECT_EQ(1U, count(rtl, "direction:rtl"));
  EXPECT_EQ(0U, count(rtl, "direction:ltr"));

  const std::string ltr =
      render(three_paragraphs("P1", R"(style:writing-mode="lr-tb")"));
  EXPECT_EQ(0U, count(ltr, "direction:ltr"));
}

namespace {

/// A page layout the master page names, carrying @p properties.
std::string page_layout_document(const std::string &properties) {
  return flat_text(R"(<text:p>one</text:p>)",
                   R"(<office:automatic-styles>)"
                   R"(<style:page-layout style:name="pm1">)"
                   R"(<style:page-layout-properties )" +
                       properties +
                       R"(/></style:page-layout>)"
                       R"(</office:automatic-styles>)"
                       R"(<office:master-styles>)"
                       R"(<style:master-page style:name="Standard")"
                       R"( style:page-layout-name="pm1"/>)"
                       R"(</office:master-styles>)");
}

} // namespace

TEST(FlatOpenDocumentFile, the_page_direction_becomes_the_root_direction) {
  EXPECT_EQ(1U,
            count(render(page_layout_document(R"(style:writing-mode="rl-tb")")),
                  R"(<html dir="rtl">)"));
  EXPECT_EQ(1U,
            count(render(page_layout_document(R"(style:writing-mode="lr-tb")")),
                  R"(<html dir="ltr">)"));
  EXPECT_EQ(1U, count(render(page_layout_document(R"(fo:page-width="21cm")")),
                      R"(<html dir="ltr">)"));
}

/// Absolute here, unlike `w:jc`'s.
TEST(FlatOpenDocumentFile, start_and_end_alignment_are_the_sides_they_name) {
  const auto align_of = [](const char *value) {
    const std::string properties =
        R"(fo:text-align=")" + std::string(value) + "\"";
    const Document document =
        DocumentFile::from_memory(three_paragraphs("P1", properties))
            .document();

    // the middle one is the one carrying `P1`
    std::vector<std::optional<TextAlign>> aligns;
    for (const Element child : document.root_element().children()) {
      aligns.push_back(child.as_paragraph().style().text_align);
    }
    return aligns.at(1);
  };

  EXPECT_EQ(TextAlign::left, align_of("start"));
  EXPECT_EQ(TextAlign::right, align_of("end"));
  EXPECT_EQ(TextAlign::left, align_of("left"));
  EXPECT_EQ(TextAlign::right, align_of("right"));

  // and a right-to-left page does not move them
  EXPECT_EQ(1U,
            count(render(three_paragraphs("P1", R"(fo:text-align="start")")),
                  "text-align:left"));
}

TEST(FlatOpenDocumentFile, it_saves_back_as_one_xml_file) {
  const Document document =
      DocumentFile::from_memory(flat_text("<text:p>Hello</text:p>")).document();

  const std::string path =
      (std::filesystem::current_path() / "flat_save_test.fodt").string();
  ASSERT_TRUE(document.is_savable());
  document.save(path);

  const DocumentFile saved(path);
  EXPECT_EQ(saved.file_type(), FileType::opendocument_text);
  EXPECT_EQ(first_of_type(saved.document().root_element(), ElementType::text)
                .as_text()
                .content(),
            "Hello");
}
