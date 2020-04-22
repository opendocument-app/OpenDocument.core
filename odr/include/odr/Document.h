#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <string>

namespace odr {

enum class FileType;
struct FileMeta;
struct Config;

class Document final {
public:
  static std::string version() noexcept;
  static std::string commit() noexcept;

  static std::unique_ptr<Document> open(const std::string &path) noexcept;
  static std::unique_ptr<Document> open(const std::string &path, FileType as) noexcept;

  static FileType readType(const std::string &path) noexcept;
  static FileMeta readMeta(const std::string &path) noexcept;

  ~Document();

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

  class Impl;
  Document(std::unique_ptr<Impl>);
private:
  const std::unique_ptr<Impl> impl_;
};

}

#endif // ODR_DOCUMENT_H
