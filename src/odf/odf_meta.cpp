#include <common/map_util.h>
#include <common/storage.h>
#include <common/storage_util.h>
#include <common/table_cursor.h>
#include <crypto/crypto_util.h>
#include <cstring>
#include <odf/odf_meta.h>
#include <odr/file.h>
#include <pugixml.hpp>

namespace odr::odf {

namespace {
bool lookupFileType(const std::string &mimeType, FileType &fileType) {
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
  return common::MapUtil::lookupMapDefault(MIME_TYPES, mimeType, fileType,
                                           FileType::UNKNOWN);
}

void estimateTableDimensions(const pugi::xml_node &table, std::uint32_t &rows,
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
      tl.addRow(repeated);
    } else if (std::strcmp(n.name(), "table:table-cell") == 0) {
      const auto repeated =
          n.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          n.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan = n.attribute("table:number-rows-spanned").as_uint(1);
      tl.addCell(colspan, rowspan, repeated);

      const auto newRows = tl.row();
      const auto newCols = std::max(cols, tl.col());
      if (n.first_child()) {
        rows = newRows;
        cols = newCols;
      }
    }
  }
}
} // namespace

FileMeta parseFileMeta(const common::ReadStorage &storage,
                       const pugi::xml_document *manifest) {
  FileMeta result;

  if (!storage.isFile("content.xml"))
    throw NoOpenDocumentFileException();

  if (storage.isFile("mimetype")) {
    const auto mimeType = common::StorageUtil::read(storage, "mimetype");
    lookupFileType(mimeType, result.type);
  }

  if (manifest != nullptr) {
    for (auto &&e : manifest->select_nodes("//manifest:file-entry")) {
      const common::Path path =
          e.node().attribute("manifest:full-path").as_string();
      if (path.root() && e.node().attribute("manifest:media-type")) {
        const std::string mimeType =
            e.node().attribute("manifest:media-type").as_string();
        lookupFileType(mimeType, result.type);
      }
    }
    if (!manifest->select_nodes("//manifest:encryption-data").empty()) {
      result.passwordEncrypted = true;
    }
  }

  return result;
}

DocumentMeta parseDocumentMeta(const pugi::xml_document *meta,
                               const pugi::xml_document &content) {
  DocumentMeta result;

  const auto body = content.document_element().child("office:body");
  if (!body)
    throw NoOpenDocumentFileException();

  if (body.child("office:text"))
    result.documentType = DocumentType::TEXT;
  else if (body.child("office:presentation"))
    result.documentType = DocumentType::PRESENTATION;
  else if (body.child("office:spreadsheet"))
    result.documentType = DocumentType::SPREADSHEET;
  else if (body.child("office:drawing"))
    result.documentType = DocumentType::DRAWING;
  // TODO else throw

  if (meta != nullptr) {
    const pugi::xml_node statistics = meta->child("office:document-meta")
                                          .child("office:meta")
                                          .child("meta:document-statistic");
    if (statistics) {
      switch (result.documentType) {
      case DocumentType::TEXT: {
        const auto pageCount = statistics.attribute("meta:page-count");
        if (!pageCount)
          break;
        result.entryCount = pageCount.as_uint();
      } break;
      case DocumentType::PRESENTATION: {
        result.entryCount = 0;
      } break;
      case DocumentType::SPREADSHEET: {
        const auto tableCount = statistics.attribute("meta:table-count");
        if (!tableCount)
          break;
        result.entryCount = tableCount.as_uint();
      } break;
      case DocumentType::DRAWING: {
      } break;
      default:
        break;
      }
    }
  }

  switch (result.documentType) {
  case DocumentType::DRAWING:
  case DocumentType::PRESENTATION: {
    result.entryCount = 0;
    for (auto &&e : body.select_nodes("//draw:page")) {
      ++result.entryCount;
      DocumentMeta::Entry entry;
      entry.name = e.node().attribute("draw:name").as_string();
      result.entries.emplace_back(entry);
    }
  } break;
  case DocumentType::SPREADSHEET: {
    result.entryCount = 0;
    for (auto &&e : body.select_nodes("//table:table")) {
      ++result.entryCount;
      DocumentMeta::Entry entry;
      entry.name = e.node().attribute("table:name").as_string();
      estimateTableDimensions(e.node(), entry.rowCount, entry.columnCount);
      result.entries.emplace_back(entry);
    }
  } break;
  default:
    break;
  }

  return result;
}

} // namespace odr::odf
