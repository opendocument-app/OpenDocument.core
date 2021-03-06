#ifndef ODR_INTERNAL_FILE_META_H
#define ODR_INTERNAL_FILE_META_H

#include <internal/document_meta.h>
#include <internal/file_type.h>
#include <optional>
#include <string>

namespace odr::internal {
enum class FileCategory;

struct FileMeta final {
  static FileType type_by_extension(const std::string &extension) noexcept;
  static FileCategory category_by_type(FileType type) noexcept;

  FileMeta();
  FileMeta(FileType type, bool password_encrypted,
           std::optional<DocumentMeta> document_meta);

  FileType type{FileType::UNKNOWN};
  bool password_encrypted{false};
  std::optional<DocumentMeta> document_meta;

  [[nodiscard]] std::string type_as_string() const noexcept;
};

} // namespace odr::internal

#endif // ODR_INTERNAL_FILE_META_H
