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
  static std::optional<DocumentNoExcept> open(const std::string &path,
                                                FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit DocumentNoExcept(Document &&);
  explicit DocumentNoExcept(std::unique_ptr<Document>);
  DocumentNoExcept(FileNoExcept &&);
  DocumentNoExcept(DocumentNoExcept &&) noexcept;
  ~DocumentNoExcept();

  using FileNoExcept::fileType;
  using FileNoExcept::fileCategory;
  using FileNoExcept::fileMeta;
  DocumentType documentType() const noexcept;
  bool encrypted() const noexcept;

  bool decrypted() const noexcept;
  bool canTranslate() const noexcept;
  bool canEdit() const noexcept;
  bool canSave() const noexcept;
  bool canSave(bool encrypted) const noexcept;

  bool decrypt(const std::string &password) const noexcept;

  bool translate(const std::string &path, const Config &config) const noexcept;
  bool edit(const std::string &diff) const noexcept;

  bool save(const std::string &path) const noexcept;
  bool save(const std::string &path,
            const std::string &password) const noexcept;

private:
  Document &document() const;
};

}

#endif // ODR_DOCUMENTNOEXCEPT_H
