#include <odr/internal/odf/odf_blank.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/blank_slide_layouts.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace odr::internal::odf {

namespace {

constexpr std::string_view root_attributes =
    R"( xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0")"
    R"( xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0")"
    R"( xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0")"
    R"( xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0")"
    R"( xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0")"
    R"( xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0")"
    R"( xmlns:xlink="http://www.w3.org/1999/xlink")"
    R"( xmlns:dc="http://purl.org/dc/elements/1.1/")"
    R"( xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0")"
    R"( xmlns:number="urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0")"
    R"( xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")"
    R"( xmlns:presentation="urn:oasis:names:tc:opendocument:xmlns:presentation:1.0")"
    R"( xmlns:of="urn:oasis:names:tc:opendocument:xmlns:of:1.2")"
    R"( office:version="1.3")";

constexpr std::string_view declaration =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    "\n";

/// A4 with margins of 2 cm, as LibreOffice sets for a new document.
constexpr std::string_view a4_page_layout =
    R"(<style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm" style:print-orientation="portrait" fo:margin-top="2cm" fo:margin-bottom="2cm" fo:margin-left="2cm" fo:margin-right="2cm" style:writing-mode="lr-tb"/>)";

constexpr std::string_view liberation_sans =
    R"(<style:font-face style:name="Liberation Sans" svg:font-family="'Liberation Sans'" style:font-family-generic="swiss" style:font-pitch="variable"/>)";

struct Blank final {
  std::string_view mimetype;
  std::string_view font_face_decls;
  std::string styles;
  std::string_view page_layout;
  std::string master_page;
  std::string body;
};

Blank text() {
  return {
      .mimetype = "application/vnd.oasis.opendocument.text",
      .font_face_decls =
          R"(<style:font-face style:name="Liberation Serif" svg:font-family="'Liberation Serif'" style:font-family-generic="roman" style:font-pitch="variable"/>)",
      .styles =
          R"(<style:default-style style:family="paragraph"><style:text-properties style:font-name="Liberation Serif" fo:font-size="12pt"/></style:default-style>)"
          R"(<style:style style:name="Standard" style:family="paragraph" style:class="text"/>)",
      .page_layout = a4_page_layout,
      .master_page =
          R"(<style:master-page style:name="Standard" style:page-layout-name="pm1"/>)",
      .body =
          R"(<office:text><text:p text:style-name="Standard"/></office:text>)",
  };
}

Blank spreadsheet() {
  return {
      .mimetype = "application/vnd.oasis.opendocument.spreadsheet",
      .font_face_decls = liberation_sans,
      .styles =
          R"(<style:default-style style:family="table-cell"><style:text-properties style:font-name="Liberation Sans" fo:font-size="10pt"/></style:default-style>)"
          R"(<style:style style:name="Default" style:family="table-cell"/>)",
      .page_layout = a4_page_layout,
      .master_page =
          R"(<style:master-page style:name="Default" style:page-layout-name="pm1"/>)",
      // ODF 1.3 part 3, 9.1.2: a table holds at least one column and one row
      .body = R"(<office:spreadsheet><table:table table:name="Sheet1">)"
              R"(<table:table-column table:default-cell-style-name="Default"/>)"
              R"(<table:table-row><table:table-cell/></table:table-row>)"
              R"(</table:table></office:spreadsheet>)",
  };
}

/// A length in hundredths of a centimetre, as ODF spells it.
std::string centimetres(const std::int32_t hundredths) {
  return fmt::format("{}cm", util::number::to_string_significant(
                                 static_cast<double>(hundredths) / 100.0, 6));
}

/// The value of `presentation:object` and `presentation:class` (ODF 1.3 part
/// 3, 19.427 and 19.388).
std::string_view presentation_class(const BlankPlaceholderKind kind) {
  switch (kind) {
  case BlankPlaceholderKind::title:
  case BlankPlaceholderKind::centered_title:
    return "title";
  case BlankPlaceholderKind::subtitle:
    return "subtitle";
  case BlankPlaceholderKind::content:
    return "outline";
  case BlankPlaceholderKind::text:
    return "text";
  }
  throw std::invalid_argument("unknown placeholder kind");
}

std::string_view presentation_style(const BlankPlaceholderKind kind) {
  switch (kind) {
  case BlankPlaceholderKind::title:
  case BlankPlaceholderKind::centered_title:
    return "Default-title";
  case BlankPlaceholderKind::subtitle:
    return "Default-subtitle";
  case BlankPlaceholderKind::content:
  case BlankPlaceholderKind::text:
    return "Default-outline1";
  }
  throw std::invalid_argument("unknown placeholder kind");
}

std::string geometry(const BlankPlaceholder &placeholder) {
  return fmt::format(R"( svg:x="{}" svg:y="{}" svg:width="{}" svg:height="{}")",
                     centimetres(placeholder.x), centimetres(placeholder.y),
                     centimetres(placeholder.width),
                     centimetres(placeholder.height));
}

/// A placeholder frame on a slide, holding one empty paragraph to type in.
std::string slide_frame(const BlankPlaceholder &placeholder) {
  return fmt::format(
      R"(<draw:frame presentation:style-name="{}"{} presentation:class="{}">)"
      R"(<draw:text-box><text:p/></draw:text-box></draw:frame>)",
      presentation_style(placeholder.kind), geometry(placeholder),
      presentation_class(placeholder.kind));
}

std::string layout_name(const std::size_t index) {
  return fmt::format("L{}", index + 1);
}

Blank presentation() {
  Blank result{
      .mimetype = "application/vnd.oasis.opendocument.presentation",
      .font_face_decls = liberation_sans,
      .styles =
          R"(<style:default-style style:family="graphic"><style:text-properties style:font-name="Liberation Sans" fo:font-size="18pt"/></style:default-style>)"
          R"(<style:style style:name="Default-title" style:family="presentation"><style:graphic-properties draw:stroke="none" draw:fill="none" draw:textarea-vertical-align="middle"/><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Liberation Sans" fo:font-size="44pt"/></style:style>)"
          R"(<style:style style:name="Default-subtitle" style:family="presentation"><style:graphic-properties draw:stroke="none" draw:fill="none"/><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Liberation Sans" fo:font-size="32pt"/></style:style>)"
          R"(<style:style style:name="Default-outline1" style:family="presentation"><style:graphic-properties draw:stroke="none" draw:fill="none"/><style:text-properties style:font-name="Liberation Sans" fo:font-size="28pt"/></style:style>)",
      .page_layout =
          R"(<style:page-layout-properties fo:page-width="28cm" fo:page-height="15.75cm" style:print-orientation="landscape" fo:margin-top="0cm" fo:margin-bottom="0cm" fo:margin-left="0cm" fo:margin-right="0cm"/>)",
      .master_page = {},
      .body = {},
  };

  for (std::size_t i = 0; i < blank_slide_layouts.size(); ++i) {
    const BlankSlideLayout &layout = blank_slide_layouts[i];
    result.styles += fmt::format(
        R"(<style:presentation-page-layout style:name="{}" style:display-name="{}">)",
        layout_name(i), layout.name);
    for (const BlankPlaceholder &placeholder : layout.placeholders) {
      result.styles += fmt::format(
          R"(<presentation:placeholder presentation:object="{}"{}/>)",
          presentation_class(placeholder.kind), geometry(placeholder));
    }
    result.styles += "</style:presentation-page-layout>";
  }

  result.master_page =
      R"(<style:master-page style:name="Default" style:page-layout-name="pm1">)";
  for (const BlankPlaceholder &placeholder : blank_master_placeholders) {
    result.master_page += fmt::format(
        R"(<draw:frame presentation:style-name="{}"{} presentation:class="{}" presentation:placeholder="true"><draw:text-box/></draw:frame>)",
        presentation_style(placeholder.kind), geometry(placeholder),
        presentation_class(placeholder.kind));
  }
  result.master_page += "</style:master-page>";

  result.body = fmt::format(
      R"(<office:presentation><draw:page draw:name="page1" draw:master-page-name="Default" presentation:presentation-page-layout-name="{}">)",
      layout_name(0));
  for (const BlankPlaceholder &placeholder :
       blank_slide_layouts[0].placeholders) {
    result.body += slide_frame(placeholder);
  }
  result.body += "</draw:page></office:presentation>";
  return result;
}

Blank find_blank(const FileType type) {
  switch (type) {
  case FileType::opendocument_text:
    return text();
  case FileType::opendocument_presentation:
    return presentation();
  case FileType::opendocument_spreadsheet:
    return spreadsheet();
  default:
    throw UnsupportedFileType(type);
  }
}

std::string manifest(const Blank &blank) {
  std::string result(declaration);
  result +=
      R"(<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.3">)";
  result +=
      R"(<manifest:file-entry manifest:full-path="/" manifest:version="1.3" manifest:media-type=")";
  result += blank.mimetype;
  result += R"("/>)";
  for (const std::string_view path :
       {"content.xml", "styles.xml", "meta.xml"}) {
    result += R"(<manifest:file-entry manifest:full-path=")";
    result += path;
    result += R"(" manifest:media-type="text/xml"/>)";
  }
  result += "</manifest:manifest>";
  return result;
}

std::string meta() {
  std::string result(declaration);
  result += "<office:document-meta";
  result += root_attributes;
  result += "><office:meta><meta:generator>odr</meta:generator></office:meta>"
            "</office:document-meta>";
  return result;
}

std::string styles(const Blank &blank) {
  std::string result(declaration);
  result += "<office:document-styles";
  result += root_attributes;
  result += "><office:font-face-decls>";
  result += blank.font_face_decls;
  result += "</office:font-face-decls><office:styles>";
  result += blank.styles;
  result +=
      R"(</office:styles><office:automatic-styles><style:page-layout style:name="pm1">)";
  result += blank.page_layout;
  result += "</style:page-layout></office:automatic-styles>"
            "<office:master-styles>";
  result += blank.master_page;
  result += "</office:master-styles></office:document-styles>";
  return result;
}

std::string content(const Blank &blank) {
  std::string result(declaration);
  result += "<office:document-content";
  result += root_attributes;
  result += "><office:font-face-decls>";
  result += blank.font_face_decls;
  result += "</office:font-face-decls><office:body>";
  result += blank.body;
  result += "</office:body></office:document-content>";
  return result;
}

} // namespace

std::string blank_package(const FileType type) {
  const Blank blank = find_blank(type);

  zip::ZipArchive archive;
  // ODF 1.3 part 2, 3.3: `mimetype` is the first file and stored
  archive.insert_file(std::end(archive), RelPath("mimetype"),
                      std::make_shared<MemoryFile>(std::string(blank.mimetype)),
                      0);
  for (auto [path, data] :
       {std::pair{"META-INF/manifest.xml", manifest(blank)},
        std::pair{"meta.xml", meta()}, std::pair{"styles.xml", styles(blank)},
        std::pair{"content.xml", content(blank)}}) {
    archive.insert_file(std::end(archive), RelPath(path),
                        std::make_shared<MemoryFile>(std::move(data)));
  }

  std::stringstream out;
  archive.save(out);
  return std::move(out).str();
}

} // namespace odr::internal::odf
