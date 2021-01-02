#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

#include <common/path.h>
#include <memory>

namespace odr {
enum class FileType;
enum class FileCategory;
struct FileMeta;
enum class FileLocation;
enum class EncryptionState;
enum class DocumentType;
struct DocumentMeta;

namespace common {
class Image;
class Archive;
class Document;

class File {
public:
  virtual ~File() = default;

  [[nodiscard]] virtual FileType fileType() const noexcept = 0;
  [[nodiscard]] virtual FileCategory fileCategory() const noexcept;
  [[nodiscard]] virtual FileMeta fileMeta() const noexcept = 0;
  [[nodiscard]] virtual FileLocation fileLocation() const noexcept = 0;

  [[nodiscard]] virtual std::size_t size() const = 0;

  [[nodiscard]] virtual std::unique_ptr<std::istream> data() const = 0;
};

class DiscFile final : public File {
public:
  explicit DiscFile(const char *path);
  explicit DiscFile(std::string path);
  explicit DiscFile(common::Path path);

  [[nodiscard]] FileType fileType() const noexcept final;
  [[nodiscard]] FileMeta fileMeta() const noexcept final;
  [[nodiscard]] FileLocation fileLocation() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] access::Path path() const;
  [[nodiscard]] std::unique_ptr<std::istream> data() const final;

private:
  common::Path m_path;
};

class MemoryFile final : public File {
public:
  explicit MemoryFile(std::string data);

  [[nodiscard]] FileType fileType() const noexcept final;
  [[nodiscard]] FileMeta fileMeta() const noexcept final;
  [[nodiscard]] FileLocation fileLocation() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] const std::string &content() const;
  [[nodiscard]] std::unique_ptr<std::istream> data() const final;

private:
  std::string m_data;
};

class ImageFile : public File {
public:
  [[nodiscard]] FileCategory fileCategory() const noexcept final;

  [[nodiscard]] virtual std::shared_ptr<Image> image() const = 0;
};

class TextFile : public File {
public:
  [[nodiscard]] FileCategory fileCategory() const noexcept final;
};

class ArchiveFile : public File {
public:
  [[nodiscard]] FileCategory fileCategory() const noexcept final;

  [[nodiscard]] virtual std::shared_ptr<Archive> archive() const = 0;
};

class DocumentFile : public File {
public:
  [[nodiscard]] virtual bool passwordEncrypted() const noexcept = 0;
  [[nodiscard]] virtual EncryptionState encryptionState() const noexcept = 0;
  [[nodiscard]] virtual bool decrypt(const std::string &password) = 0;

  [[nodiscard]] virtual DocumentType documentType() const;
  [[nodiscard]] virtual DocumentMeta documentMeta() const;

  [[nodiscard]] virtual std::shared_ptr<Document> document() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_FILE_H
