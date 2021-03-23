#include <internal/util/odr_meta_util.h>
#include <odr/file.h>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const FileMeta &meta) {
  nlohmann::json result;

  result["type"] = meta.type_as_string();
  result["encrypted"] = meta.password_encrypted;
  result["entryCount"] = 0;
  result["entries"] = nlohmann::json::array();

  if (meta.document_meta) {
    auto &&document_meta = *meta.document_meta;

    result["entryCount"] = document_meta.entry_count;
  }

  return result;
}

} // namespace odr::internal::util::meta
