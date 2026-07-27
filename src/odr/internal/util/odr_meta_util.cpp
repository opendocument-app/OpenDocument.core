#include <odr/internal/util/odr_meta_util.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <nlohmann/json.hpp>

namespace odr::internal::util::meta {

nlohmann::json meta_to_json(const FileMeta &meta) {
  nlohmann::json result;

  result["fileType"] = file_type_to_string(meta.type);
  result["fileCategory"] =
      file_category_to_string(file_category_by_file_type(meta.type));
  result["isEncrypted"] = meta.password_encrypted;

  if (meta.document_type != DocumentType::unknown) {
    result["documentType"] = document_type_to_string(meta.document_type);

    if (meta.entry_count) {
      result["entryCount"] = *meta.entry_count;
    }

    const auto put = [&](const char *key,
                         const std::optional<std::string> &value) {
      if (value) {
        result[key] = *value;
      }
    };
    put("title", meta.title);
    put("author", meta.author);
    put("subject", meta.subject);
    put("keywords", meta.keywords);
    put("creator", meta.creator);
    put("producer", meta.producer);
    put("creationDate", meta.creation_date);
    put("modificationDate", meta.modification_date);
  }

  return result;
}

} // namespace odr::internal::util::meta
