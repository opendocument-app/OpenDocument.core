#ifndef ODR_FILE_NO_EXCEPT_H
#define ODR_FILE_NO_EXCEPT_H

#include <memory>
#include <odr/file.h>
#include <optional>

namespace odr {

class FileNoExcept {
public:
  static std::optional<FileNoExcept> open(const std::string &path) noexcept;
  static std::optional<FileNoExcept> open(const std::string &path,
                                          FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit FileNoExcept(File);

  virtual FileType file_type() const noexcept;
  virtual FileCategory file_category() const noexcept;
  virtual FileMeta file_meta() const noexcept;

protected:
  File m_file;
};

class DocumentFileNoExcept : public FileNoExcept {
public:
  static std::optional<DocumentFileNoExcept>
  open(const std::string &path) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit DocumentFileNoExcept(DocumentFile);

  DocumentType document_type() const noexcept;

  bool editable() const noexcept;
  bool savable(bool encrypted) const noexcept;

  bool save(const std::string &path) const noexcept;
  bool save(const std::string &path,
            const std::string &password) const noexcept;

protected:
  DocumentFile m_document_file;
};

} // namespace odr

#endif // ODR_FILE_NO_EXCEPT_H
