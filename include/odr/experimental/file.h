#ifndef ODR_EXPERIMENTAL_FILE_H
#define ODR_EXPERIMENTAL_FILE_H

#include <cstdint>
#include <memory>
#include <odr/file_type.h>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class File;
class DecodedFile;
class ImageFile;
class DocumentFile;
} // namespace odr::internal::abstract

namespace odr::experimental {
enum class FileCategory;
enum class FileLocation;
enum class EncryptionState;
struct FileMeta;
class ImageFile;
enum class DocumentType;
class DocumentFile;
struct DocumentMeta;
class Document;

class File {
public:
  explicit File(std::shared_ptr<internal::abstract::File>);
  explicit File(const std::string &path);

  [[nodiscard]] FileLocation location() const noexcept;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::unique_ptr<std::istream> read() const;

  // TODO `impl()` might be a bit dirty
  [[nodiscard]] std::shared_ptr<internal::abstract::File> impl() const;

protected:
  std::shared_ptr<internal::abstract::File> m_impl;
};

class DecodedFile {
public:
  static std::vector<FileType> types(const std::string &path);
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DecodedFile(std::shared_ptr<internal::abstract::DecodedFile>);
  explicit DecodedFile(const std::string &path);
  DecodedFile(const std::string &path, FileType as);

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] FileCategory file_category() const noexcept;
  [[nodiscard]] FileMeta file_meta() const noexcept;

  [[nodiscard]] ImageFile image_file() const;
  [[nodiscard]] DocumentFile document_file() const;

protected:
  std::shared_ptr<internal::abstract::DecodedFile> m_impl;
};

class ImageFile : public DecodedFile {
public:
  explicit ImageFile(std::shared_ptr<internal::abstract::ImageFile>);

private:
  std::shared_ptr<internal::abstract::ImageFile> m_impl;
};

class DocumentFile : public DecodedFile {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DocumentFile(std::shared_ptr<internal::abstract::DocumentFile>);
  explicit DocumentFile(const std::string &path);

  [[nodiscard]] bool password_encrypted() const;
  [[nodiscard]] EncryptionState encryption_state() const;
  bool decrypt(const std::string &password);

  [[nodiscard]] DocumentType document_type() const;
  [[nodiscard]] DocumentMeta document_meta() const;

  [[nodiscard]] Document document() const;

private:
  std::shared_ptr<internal::abstract::DocumentFile> m_impl;
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_FILE_H
