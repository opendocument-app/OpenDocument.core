#include <odr/internal/odf/odf_meta.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <string_view>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {

void lookup_file_type(const std::string &mimetype_in, FileType &file_type,
                      std::string_view &mimetype_out) {
  // https://www.openoffice.org/framework/documentation/mimetypes/mimetypes.html
  static const std::unordered_map<std::string, FileType> MIME_TYPES = {
      {"application/vnd.oasis.opendocument.text", FileType::opendocument_text},
      {"application/vnd.oasis.opendocument.presentation",
       FileType::opendocument_presentation},
      {"application/vnd.oasis.opendocument.spreadsheet",
       FileType::opendocument_spreadsheet},
      {"application/vnd.oasis.opendocument.graphics",
       FileType::opendocument_graphics},
      // TODO any difference for template files?
      {"application/vnd.oasis.opendocument.text-template",
       FileType::opendocument_text},
      {"application/vnd.oasis.opendocument.text-master",
       FileType::opendocument_text},
      {"application/vnd.oasis.opendocument.presentation-template",
       FileType::opendocument_presentation},
      {"application/vnd.oasis.opendocument.spreadsheet-template",
       FileType::opendocument_spreadsheet},
      {"application/vnd.oasis.opendocument.graphics-template",
       FileType::opendocument_graphics},
      // TODO these staroffice types might deserve their own type
      {"application/vnd.sun.xml.writer", FileType::opendocument_text},
      {"application/vnd.sun.xml.impress", FileType::opendocument_presentation},
      {"application/vnd.sun.xml.calc", FileType::opendocument_spreadsheet},
      {"application/vnd.sun.xml.draw", FileType::opendocument_graphics},
      // TODO any difference for template files?
      {"application/vnd.sun.xml.writer.template", FileType::opendocument_text},
      {"application/vnd.sun.xml.impress.template",
       FileType::opendocument_presentation},
      {"application/vnd.sun.xml.calc.template",
       FileType::opendocument_spreadsheet},
      {"application/vnd.sun.xml.draw.template",
       FileType::opendocument_graphics},
      // a flat document's root names the packaged mimetype; `-flat-xml` is
      // what a caller may name the file
      {"application/vnd.oasis.opendocument.text-flat-xml",
       FileType::opendocument_text},
      {"application/vnd.oasis.opendocument.presentation-flat-xml",
       FileType::opendocument_presentation},
      {"application/vnd.oasis.opendocument.spreadsheet-flat-xml",
       FileType::opendocument_spreadsheet},
      {"application/vnd.oasis.opendocument.graphics-flat-xml",
       FileType::opendocument_graphics},
  };
  if (const auto it = MIME_TYPES.find(mimetype_in); it != MIME_TYPES.end()) {
    file_type = it->second;
    mimetype_out = it->first;
  } else {
    file_type = FileType::unknown;
    mimetype_out = "application/octet-stream";
  }
}

std::string_view flat_mimetype(const FileType file_type) {
  switch (file_type) {
  case FileType::opendocument_text:
    return "application/vnd.oasis.opendocument.text-flat-xml";
  case FileType::opendocument_presentation:
    return "application/vnd.oasis.opendocument.presentation-flat-xml";
  case FileType::opendocument_spreadsheet:
    return "application/vnd.oasis.opendocument.spreadsheet-flat-xml";
  case FileType::opendocument_graphics:
    return "application/vnd.oasis.opendocument.graphics-flat-xml";
  default:
    return "application/octet-stream";
  }
}

void read_entry_count(const pugi::xml_node statistics, FileMeta &result) {
  const char *attribute = nullptr;
  if (result.type == FileType::opendocument_text) {
    attribute = "meta:page-count";
  } else if (result.type == FileType::opendocument_spreadsheet) {
    attribute = "meta:table-count";
  } else {
    return;
  }

  if (const pugi::xml_attribute count = statistics.attribute(attribute)) {
    result.entry_count = count.as_uint();
  }
}

} // namespace

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                         const pugi::xml_document *manifest,
                         const bool decrypted) {
  FileMeta result;

  result.password_encrypted = decrypted;

  if (!filesystem.is_file(AbsPath("/content.xml")) && manifest == nullptr &&
      !filesystem.is_file(AbsPath("/mimetype"))) {
    throw NoOpenDocumentFile();
  }

  if (filesystem.is_file(AbsPath("/mimetype"))) {
    const std::string mimeType =
        util::stream::read(*filesystem.open(AbsPath("/mimetype"))->stream());
    lookup_file_type(mimeType, result.type, result.mimetype);
  }

  pugi::xml_document manifest_xml;
  if (manifest == nullptr &&
      filesystem.is_file(AbsPath("/META-INF/manifest.xml"))) {
    manifest_xml = xml::parse(filesystem, AbsPath("/META-INF/manifest.xml"));
    manifest = &manifest_xml;
  }

  if (manifest != nullptr) {
    for (const pugi::xpath_node e :
         manifest->select_nodes("//manifest:file-entry")) {
      if (const Path path(e.node().attribute("manifest:full-path").as_string());
          path.root() && e.node().attribute("manifest:media-type")) {
        const std::string mimeType =
            e.node().attribute("manifest:media-type").as_string();
        lookup_file_type(mimeType, result.type, result.mimetype);
      }
    }
    if (!manifest->select_nodes("//manifest:encryption-data").empty()) {
      result.password_encrypted = true;
    }
  }

  result.document_type = document_type_by_file_type(result.type);

  if (result.password_encrypted == decrypted &&
      filesystem.is_file(AbsPath("/meta.xml"))) {
    const pugi::xml_document meta_xml =
        xml::parse(filesystem, AbsPath("/meta.xml"));

    const pugi::xml_node statistics = meta_xml.child("office:document-meta")
                                          .child("office:meta")
                                          .child("meta:document-statistic");

    read_entry_count(statistics, result);
  }

  return result;
}

FileMeta parse_flat_file_meta(const pugi::xml_node root) {
  if (std::string_view(root.name()) != "office:document") {
    throw NoOpenDocumentFile();
  }

  FileMeta result;
  lookup_file_type(root.attribute("office:mimetype").value(), result.type,
                   result.mimetype);
  if (result.type == FileType::unknown) {
    throw NoOpenDocumentFile();
  }
  result.mimetype = flat_mimetype(result.type);
  result.document_type = document_type_by_file_type(result.type);

  read_entry_count(root.child("office:meta").child("meta:document-statistic"),
                   result);

  return result;
}

} // namespace odr::internal::odf
