#include <internal/util/odr_meta_util.h>
#include <nlohmann/json.hpp>
#include <odr/file.h>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const FileMeta &meta) {
  nlohmann::json result;

  result["type"] = meta.type_as_string();
  result["encrypted"] = meta.password_encrypted;

  if (meta.document_meta) {
    auto &&document_meta = *meta.document_meta;

    if (document_meta.entry_count) {
      result["entryCount"] = *document_meta.entry_count;
    }
  }

  return result;
}

} // namespace odr::internal::util::meta
