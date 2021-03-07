#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/table_cursor.h>
#include <internal/odf/odf_meta.h>
#include <internal/util/map_util.h>
#include <internal/util/stream_util.h>
#include <odr/exceptions.h>
#include <odr/experimental/file_meta.h>
#include <odr/file_type.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

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
} // namespace

experimental::FileMeta
parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                const pugi::xml_document *manifest) {
  experimental::FileMeta result;

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

void estimate_table_dimensions(const pugi::xml_node &table, std::uint32_t &rows,
                               std::uint32_t &cols) {
  rows = 0;
  cols = 0;

  common::TableCursor tl;

  for (auto &&r : table.select_nodes(".//self::table:table-row")) {
    const auto &&row = r.node();

    const auto rows_repeated =
        row.attribute("table:number-rows-repeated").as_uint(1);
    tl.add_row(rows_repeated);

    for (auto &&c : row.select_nodes(".//self::table:table-cell")) {
      const auto &&cell = c.node();

      const auto columns_repeated =
          cell.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          cell.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan =
          cell.attribute("table:number-rows-spanned").as_uint(1);
      tl.add_cell(colspan, rowspan, columns_repeated);

      const auto new_rows = tl.row();
      const auto new_cols = std::max(cols, tl.col());
      if (cell.first_child()) {
        rows = new_rows;
        cols = new_cols;
      }
    }
  }
}

} // namespace odr::internal::odf
