#include <odr/internal/util/odr_meta_util.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <nlohmann/json.hpp>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const FileMeta &meta) {
  nlohmann::json result;

  result["type"] = odr::type_to_string(meta.type);
  result["encrypted"] = meta.password_encrypted;

  if (meta.document_meta) {
    const auto &document_meta = *meta.document_meta;

    if (document_meta.entry_count) {
      result["entryCount"] = *document_meta.entry_count;
    }
  }

  return result;
}

} // namespace odr::internal::util::meta
