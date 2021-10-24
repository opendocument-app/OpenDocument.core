#ifndef ODR_INTERNAL_ABSTRACT_FILE_H
#define ODR_INTERNAL_ABSTRACT_FILE_H

#include <iosfwd>
#include <memory>
#include <odr/file.h>
#include <string>

namespace odr::internal::abstract {
class Image;
class Archive;
class Document;

class File {
public:
  virtual ~File() = default;

  [[nodiscard]] virtual FileLocation location() const noexcept = 0;
  [[nodiscard]] virtual std::size_t size() const = 0;
  [[nodiscard]] virtual std::unique_ptr<std::istream> read() const = 0;
};

class DecodedFile {
public:
  virtual ~DecodedFile() = default;

  [[nodiscard]] virtual std::shared_ptr<File> file() const noexcept = 0;

  [[nodiscard]] virtual FileType file_type() const noexcept = 0;
  [[nodiscard]] virtual FileCategory file_category() const noexcept = 0;
  [[nodiscard]] virtual FileMeta file_meta() const noexcept = 0;
};

class TextFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::text;
  }
};

class ImageFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::image;
  }

  [[nodiscard]] virtual std::shared_ptr<Image> image() const = 0;
};

class ArchiveFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::archive;
  }

  [[nodiscard]] virtual std::shared_ptr<Archive> archive() const = 0;
};

class DocumentFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::document;
  }

  [[nodiscard]] virtual bool password_encrypted() const noexcept = 0;
  [[nodiscard]] virtual EncryptionState encryption_state() const noexcept = 0;
  [[nodiscard]] virtual bool decrypt(const std::string &password) = 0;

  [[nodiscard]] virtual DocumentType document_type() const = 0;
  [[nodiscard]] virtual DocumentMeta document_meta() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document> document() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_FILE_H
