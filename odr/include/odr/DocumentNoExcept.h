#ifndef ODR_DOCUMENTNOEXCEPT_H
#define ODR_DOCUMENTNOEXCEPT_H

#include <odr/Document.h>

namespace odr {

class DocumentNoExcept final {
public:
  static std::unique_ptr<DocumentNoExcept>
  open(const std::string &path) noexcept;
  static std::unique_ptr<DocumentNoExcept> open(const std::string &path,
                                                FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit DocumentNoExcept(Document &&);
  DocumentNoExcept(DocumentNoExcept &&) noexcept;
  ~DocumentNoExcept();

  FileType type() const noexcept;
  bool encrypted() const noexcept;
  const FileMeta &meta() const noexcept;

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
  Document impl_;
};

}

#endif // ODR_DOCUMENTNOEXCEPT_H
