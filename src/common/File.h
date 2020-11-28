#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

#include <memory>

namespace odr {
enum class FileType;
enum class FileCategory;
struct FileMeta;
enum class DocumentType;
struct DocumentMeta;

namespace common {
class Document;

class File {
public:
  virtual ~File() = default;

  virtual FileType fileType() const noexcept = 0;
  virtual FileCategory fileCategory() const noexcept = 0;
  virtual FileMeta fileMeta() const noexcept = 0;
};

class DocumentFile : public File {
public:
  virtual DocumentType documentType() const noexcept;
  virtual DocumentMeta documentMeta() const noexcept;

  virtual std::shared_ptr<Document> document() const = 0;
};

}
}

#endif // ODR_COMMON_FILE_H
