#include <internal/util/odr_meta_util.h>
#include <odr/experimental/file_meta.h>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const experimental::FileMeta &meta) {
  nlohmann::json result;

  result["type"] = meta.type_as_string();
  result["encrypted"] = meta.password_encrypted;
  result["entryCount"] = 0;
  result["entries"] = nlohmann::json::array();

  if (meta.document_meta) {
    auto &&document_meta = *meta.document_meta;

    result["entryCount"] = document_meta.entry_count;

    for (auto &&e : document_meta.entries) {
      nlohmann::json entry;

      entry["name"] = "";
      entry["rowCount"] = 0;
      entry["columnCount"] = 0;
      entry["notes"] = "";

      if (e.name) {
        entry["name"] = *e.name;
      }
      if (e.table_dimensions) {
        entry["rowCount"] = e.table_dimensions->rows;
        entry["columnCount"] = e.table_dimensions->columns;
      }
      if (e.notes) {
        entry["notes"] = *e.notes;
      }

      result["entries"].push_back(entry);
    }
  }

  return result;
}

} // namespace odr::internal::util::meta
