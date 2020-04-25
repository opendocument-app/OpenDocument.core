#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <optional>
#include <string>

namespace odr {

enum class FileType;
struct FileMeta;
struct Config;

class Document final {
public:
  static std::string version() noexcept;
  static std::string commit() noexcept;

  static FileType readType(const std::string &path);
  static FileMeta readMeta(const std::string &path);

  explicit Document(const std::string &path);
  Document(const std::string &path, FileType as);
  Document(Document &&) noexcept;
  ~Document();

  FileType type() const noexcept;
  bool encrypted() const noexcept;
  const FileMeta &meta() const noexcept;

  bool decrypted() const noexcept;
  bool canTranslate() const noexcept;
  bool canEdit() const noexcept;
  bool canSave() const noexcept;
  bool canSave(bool encrypted) const noexcept;

  bool decrypt(const std::string &password) const;

  void translate(const std::string &path, const Config &config) const;
  void edit(const std::string &diff) const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  class Impl;
private:
  std::unique_ptr<Impl> impl_;
};

class DocumentNoExcept final {
public:
  static std::optional<DocumentNoExcept> open(const std::string &path) noexcept;
  static std::optional<DocumentNoExcept> open(const std::string &path, FileType as) noexcept;

  static FileType readType(const std::string &path) noexcept;
  static FileMeta readMeta(const std::string &path) noexcept;

  explicit DocumentNoExcept(std::unique_ptr<Document>);
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
  bool save(const std::string &path, const std::string &password) const
  noexcept;

private:
  std::unique_ptr<Document> impl_;
};

}

#endif // ODR_DOCUMENT_H
