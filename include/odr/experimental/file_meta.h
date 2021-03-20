#ifndef ODR_EXPERIMENTAL_FILE_META_H
#define ODR_EXPERIMENTAL_FILE_META_H

#include <odr/experimental/document_meta.h>
#include <odr/file_type.h>
#include <optional>
#include <string>

namespace odr {
enum class FileCategory;
}

namespace odr::experimental {

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

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_FILE_META_H
