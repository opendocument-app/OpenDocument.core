#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using namespace odr;

namespace {

/// A flat document needs no namespace declarations to parse - prefixes are
/// part of the tag name and nothing resolves them.
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

/// The flat mimetypes are what a caller names the file, not what the root
/// carries - both have to open.
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
/// has both under its one root, and they have to resolve just the same.
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

/// An `xlink:href` still names a file in a package - a flat document has none,
/// so the image is external and stays a plain link.
TEST(FlatOpenDocumentFile, a_linked_image_is_not_internal) {
  const Document document =
      DocumentFile::from_memory(
          flat_text(R"(<text:p><draw:frame><draw:image )"
                    R"(xlink:href="https://example.org/a.png"/>)"
                    R"(</draw:frame></text:p>)"))
          .document();

  const Image image =
      first_of_type(document.root_element(), ElementType::image).as_image();
  EXPECT_FALSE(image.is_internal());
  EXPECT_EQ(image.href(), "https://example.org/a.png");
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
