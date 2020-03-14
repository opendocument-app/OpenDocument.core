#ifndef ODR_OPEN_DOCUMENT_READER_H
#define ODR_OPEN_DOCUMENT_READER_H

#include <memory>
#include <string>

namespace odr {

enum class FileType;
struct FileMeta;
struct Config;

class OpenDocumentReader final {
public:
  static std::string getVersion() noexcept;
  static std::string getCommit() noexcept;

  OpenDocumentReader();
  ~OpenDocumentReader();

  FileType guess(const std::string &path) const noexcept;

  bool open(const std::string &path) const noexcept;
  bool open(const std::string &path, FileType as) const noexcept;
  bool save(const std::string &path) const noexcept;
  bool save(const std::string &path, const std::string &password) const
      noexcept;
  void close() const noexcept;

  bool isOpen() const noexcept;
  bool isDecrypted() const noexcept;
  bool canTranslate() const noexcept;
  bool canEdit() const noexcept;
  bool canSave() const noexcept;
  bool canSave(bool encrypted) const noexcept;
  const FileMeta &getMeta() const noexcept;

  bool decrypt(const std::string &password) const noexcept;

  bool translate(const std::string &path, const Config &config) const noexcept;
  bool edit(const std::string &diff) const noexcept;

private:
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_H
