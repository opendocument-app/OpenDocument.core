#include <abstract/file.h>
#include <abstract/filesystem.h>
#include <common/table_cursor.h>
#include <crypto/crypto_util.h>
#include <cstring>
#include <odf/odf_meta.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <pugixml.hpp>
#include <util/map_util.h>
#include <util/stream_util.h>

namespace odr::odf {

namespace {
bool lookup_file_type(const std::string &mime_type, FileType &file_type) {
  // https://www.openoffice.org/framework/documentation/mimetypes/mimetypes.html
  static const std::unordered_map<std::string, FileType> MIME_TYPES = {
      {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.oasis.opendocument.presentation",
       FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.oasis.opendocument.spreadsheet",
       FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.oasis.opendocument.graphics",
       FileType::OPENDOCUMENT_GRAPHICS},
      // TODO any difference for template files?
      {"application/vnd.oasis.opendocument.text-template",
       FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.oasis.opendocument.presentation-template",
       FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.oasis.opendocument.spreadsheet-template",
       FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.oasis.opendocument.graphics-template",
       FileType::OPENDOCUMENT_GRAPHICS},
  };
  return util::map::lookup_map_default(MIME_TYPES, mime_type, file_type,
                                       FileType::UNKNOWN);
}

void estimate_table_dimensions(const pugi::xml_node &table, std::uint32_t &rows,
                               std::uint32_t &cols) {
  rows = 0;
  cols = 0;

  common::TableCursor tl;

  auto range = table.select_nodes(
      ".//self::table:table-row | .//self::table:table-cell");
  range.sort();
  for (auto &&e : range) {
    const auto &&n = e.node();
    if (std::strcmp(n.name(), "table:table-row") == 0) {
      const auto repeated =
          n.attribute("table:number-rows-repeated").as_uint(1);
      tl.add_row(repeated);
    } else if (std::strcmp(n.name(), "table:table-cell") == 0) {
      const auto repeated =
          n.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          n.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan = n.attribute("table:number-rows-spanned").as_uint(1);
      tl.add_cell(colspan, rowspan, repeated);

      const auto new_rows = tl.row();
      const auto new_cols = std::max(cols, tl.col());
      if (n.first_child()) {
        rows = new_rows;
        cols = new_cols;
      }
    }
  }
}
} // namespace

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                         const pugi::xml_document *manifest) {
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

  return result;
}

DocumentMeta parse_document_meta(const pugi::xml_document *meta,
                                 const pugi::xml_document &content) {
  DocumentMeta result;

  const auto body = content.document_element().child("office:body");
  if (!body) {
    throw NoOpenDocumentFile();
  }

  if (body.child("office:text")) {
    result.document_type = DocumentType::TEXT;
  } else if (body.child("office:presentation")) {
    result.document_type = DocumentType::PRESENTATION;
  } else if (body.child("office:spreadsheet")) {
    result.document_type = DocumentType::SPREADSHEET;
  } else if (body.child("office:drawing")) {
    result.document_type = DocumentType::DRAWING;
  }
  // TODO else throw

  if (meta != nullptr) {
    const pugi::xml_node statistics = meta->child("office:document-meta")
                                          .child("office:meta")
                                          .child("meta:document-statistic");
    if (statistics) {
      switch (result.document_type) {
      case DocumentType::TEXT: {
        const auto page_count = statistics.attribute("meta:page-count");
        if (!page_count) {
          break;
        }
        result.entry_count = page_count.as_uint();
      } break;
      case DocumentType::PRESENTATION: {
        result.entry_count = 0;
      } break;
      case DocumentType::SPREADSHEET: {
        const auto table_count = statistics.attribute("meta:table-count");
        if (!table_count) {
          break;
        }
        result.entry_count = table_count.as_uint();
      } break;
      case DocumentType::DRAWING: {
      } break;
      default:
        break;
      }
    }
  }

  switch (result.document_type) {
  case DocumentType::DRAWING:
  case DocumentType::PRESENTATION: {
    result.entry_count = 0;
    for (auto &&e : body.select_nodes("//draw:page")) {
      ++result.entry_count;
      DocumentMeta::Entry entry;
      entry.name = e.node().attribute("draw:name").as_string();
      result.entries.emplace_back(entry);
    }
  } break;
  case DocumentType::SPREADSHEET: {
    result.entry_count = 0;
    for (auto &&e : body.select_nodes("//table:table")) {
      ++result.entry_count;
      DocumentMeta::Entry entry;
      entry.name = e.node().attribute("table:name").as_string();
      estimate_table_dimensions(e.node(), entry.row_count, entry.column_count);
      result.entries.emplace_back(entry);
    }
  } break;
  default:
    break;
  }

  return result;
}

} // namespace odr::odf
