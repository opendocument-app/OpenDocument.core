#ifndef ODR_DOCUMENTNOEXCEPT_H
#define ODR_DOCUMENTNOEXCEPT_H

#include <optional>
#include <odr/Document.h>
#include <odr/FileNoExcept.h>

namespace odr {

class DocumentNoExcept : public FileNoExcept {
public:
  static std::optional<DocumentNoExcept>
  open(const std::string &path) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit DocumentNoExcept(Document &&);
  explicit DocumentNoExcept(std::unique_ptr<Document>);
  DocumentNoExcept(FileNoExcept &&);

  using FileNoExcept::fileType;
  using FileNoExcept::fileCategory;
  using FileNoExcept::fileMeta;
  DocumentType documentType() const noexcept;

  bool editable() const noexcept;
  bool savable(bool encrypted) const noexcept;

  bool save(const std::string &path) const noexcept;
  bool save(const std::string &path,
            const std::string &password) const noexcept;

private:
  Document &impl() const noexcept;
};

}

#endif // ODR_DOCUMENTNOEXCEPT_H
