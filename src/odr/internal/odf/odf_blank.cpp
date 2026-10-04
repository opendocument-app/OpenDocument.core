#include <odr/internal/odf/odf_blank.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

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
    R"( xmlns:of="urn:oasis:names:tc:opendocument:xmlns:of:1.2")"
    R"( office:version="1.3")";

constexpr std::string_view declaration =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    "\n";

struct Blank final {
  std::string_view mimetype;
  std::string_view font_face_decls;
  std::string_view styles;
  std::string_view page_layout;
  std::string_view body;
};

constexpr Blank text{
    .mimetype = "application/vnd.oasis.opendocument.text",
    .font_face_decls =
        R"(<style:font-face style:name="Liberation Serif" svg:font-family="'Liberation Serif'" style:font-family-generic="roman" style:font-pitch="variable"/>)",
    .styles =
        R"(<style:default-style style:family="paragraph"><style:text-properties style:font-name="Liberation Serif" fo:font-size="12pt"/></style:default-style>)"
        R"(<style:style style:name="Standard" style:family="paragraph" style:class="text"/>)",
    .page_layout =
        R"(<style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm" style:print-orientation="portrait" fo:margin-top="2cm" fo:margin-bottom="2cm" fo:margin-left="2cm" fo:margin-right="2cm" style:writing-mode="lr-tb"/>)",
    .body =
        R"(<office:text><text:p text:style-name="Standard"/></office:text>)",
};

const Blank &find_blank(const FileType type) {
  switch (type) {
  case FileType::opendocument_text:
    return text;
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
  result +=
      "</style:page-layout></office:automatic-styles>"
      R"(<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1"/></office:master-styles>)"
      "</office:document-styles>";
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
  const Blank &blank = find_blank(type);

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
