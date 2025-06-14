#include <odr/internal/util/odr_meta_util.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <nlohmann/json.hpp>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const FileMeta &meta) {
  nlohmann::json result;

  result["fileType"] = odr::file_type_to_string(meta.type);
  result["fileCategory"] =
      odr::file_category_to_string(odr::file_category_by_file_type(meta.type));
  result["isEncrypted"] = meta.password_encrypted;

  if (meta.document_meta) {
    const auto &document_meta = *meta.document_meta;

    result["documentType"] =
        odr::document_type_to_string(document_meta.document_type);

    if (document_meta.entry_count) {
      result["entryCount"] = *document_meta.entry_count;
    }
  }

  return result;
}

} // namespace odr::internal::util::meta
