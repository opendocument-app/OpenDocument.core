#ifndef ODR_READER_H
#define ODR_READER_H

#include <memory>
#include <string>

namespace odr {

enum class FileType;
struct FileMeta;
struct Config;

class Reader final {
public:
  static std::string version() noexcept;
  static std::string commit() noexcept;

  static FileType readType(const std::string &path) noexcept;
  static FileMeta readMeta(const std::string &path) noexcept;

  Reader();
  ~Reader();

  bool open() const noexcept;
  bool decrypted() const noexcept;
  bool canTranslate() const noexcept;
  bool canEdit() const noexcept;
  bool canSave() const noexcept;
  bool canSave(bool encrypted) const noexcept;

  bool encrypted() const noexcept;
  FileType type() const noexcept;
  const FileMeta &meta() const noexcept;

  bool open(const std::string &path) const noexcept;
  bool open(const std::string &path, FileType as) const noexcept;

  bool decrypt(const std::string &password) const noexcept;

  bool translate(const std::string &path, const Config &config) const noexcept;
  bool edit(const std::string &diff) const noexcept;

  bool save(const std::string &path) const noexcept;
  bool save(const std::string &path, const std::string &password) const
      noexcept;

  void close() const noexcept;

private:
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

} // namespace odr

#endif // ODR_READER_H
