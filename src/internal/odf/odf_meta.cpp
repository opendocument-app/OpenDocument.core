#include <algorithm>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/odf/odf_meta.h>
#include <internal/util/map_util.h>
#include <internal/util/stream_util.h>
#include <internal/util/xml_util.h>
#include <memory>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <pugixml.hpp>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

namespace {
bool lookup_file_type(const std::string &mime_type, FileType &file_type) {
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
  };
  return util::map::lookup_map_default(MIME_TYPES, mime_type, file_type,
                                       FileType::unknown);
}
} // namespace

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                         const pugi::xml_document *manifest,
                         const bool decrypted) {
  FileMeta result;

  if (!filesystem.is_file("content.xml")) {
    throw NoOpenDocumentFile();
  }

  if (filesystem.is_file("mimetype")) {
    const auto mimeType =
        util::stream::read(*filesystem.open("mimetype")->read());
    lookup_file_type(mimeType, result.type);
  }

  if (manifest != nullptr) {
    for (auto &&e : manifest->select_nodes("//manifest:file-entry")) {
      const common::Path path =
          e.node().attribute("manifest:full-path").as_string();
      if (path.root() && e.node().attribute("manifest:media-type")) {
        const std::string mimeType =
            e.node().attribute("manifest:media-type").as_string();
        lookup_file_type(mimeType, result.type);
      }
    }
    if (!manifest->select_nodes("//manifest:encryption-data").empty()) {
      result.password_encrypted = true;
    }
  }

  DocumentMeta document_meta;

  if (result.password_encrypted == decrypted) {
    if (filesystem.is_file("meta.xml")) {
      const auto meta_xml = util::xml::parse(filesystem, "meta.xml");

      const pugi::xml_node statistics = meta_xml.child("office:document-meta")
                                            .child("office:meta")
                                            .child("meta:document-statistic");
      if (statistics) {
        switch (result.type) {
        case FileType::opendocument_text: {
          document_meta.document_type = DocumentType::text;
          const auto page_count = statistics.attribute("meta:page-count");
          if (!page_count) {
            break;
          }
          document_meta.entry_count = page_count.as_uint();
        } break;
        case FileType::opendocument_presentation: {
          document_meta.document_type = DocumentType::presentation;
        } break;
        case FileType::opendocument_spreadsheet: {
          document_meta.document_type = DocumentType::spreadsheet;
          const auto table_count = statistics.attribute("meta:table-count");
          if (!table_count) {
            break;
          }
          document_meta.entry_count = table_count.as_uint();
        } break;
        case FileType::opendocument_graphics: {
          document_meta.document_type = DocumentType::drawing;
        } break;
        default:
          break;
        }
      }
    }
  }

  result.document_meta = std::move(document_meta);

  return result;
}

} // namespace odr::internal::odf
