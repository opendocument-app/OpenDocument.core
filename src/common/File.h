#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

#include <memory>

namespace odr {
enum class FileType;
enum class FileCategory;
struct FileMeta;
enum class EncryptionState;
enum class DocumentType;
struct DocumentMeta;

namespace common {
class Document;

class File {
public:
  virtual ~File() = default;

  virtual FileType fileType() const noexcept = 0;
  virtual FileCategory fileCategory() const noexcept;
  virtual FileMeta fileMeta() const noexcept = 0;
};

class DocumentFile : public File {
public:
  virtual bool passwordEncrypted() const noexcept = 0;
  virtual EncryptionState encryptionState() const noexcept = 0;
  virtual bool decrypt(const std::string &password) = 0;

  virtual DocumentType documentType() const;
  virtual DocumentMeta documentMeta() const;

  virtual std::shared_ptr<Document> document() const = 0;
};

}
}

#endif // ODR_COMMON_FILE_H
