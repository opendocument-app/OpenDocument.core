#include <internal/util/odr_meta_util.h>
#include <odr/file_meta.h>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const odr::FileMeta &meta) {
  nlohmann::json result;

  result["type"] = meta.type_as_string();
  result["encrypted"] = meta.encrypted;
  result["entryCount"] = 0;
  result["entries"] = nlohmann::json::array();

  {
    result["entryCount"] = meta.entry_count;

    for (auto &&e : meta.entries) {
      nlohmann::json entry;

      entry["name"] = "";
      entry["rowCount"] = 0;
      entry["columnCount"] = 0;
      entry["notes"] = "";

      { entry["name"] = e.name; }
      {
        entry["rowCount"] = e.row_count;
        entry["columnCount"] = e.column_count;
      }
      { entry["notes"] = e.notes; }

      result["entries"].push_back(entry);
    }
  }

  return result;
}

} // namespace odr::internal::util::meta
