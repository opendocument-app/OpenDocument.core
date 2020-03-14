#ifndef ODR_OPENDOCUMENTREADER_H
#define ODR_OPENDOCUMENTREADER_H

#include <memory>
#include <string>

namespace odr {

enum class FileType;
struct FileMeta;
struct Config;

class OpenDocumentReader final {
public:
  static std::string getVersion();
  static std::string getCommit();

  OpenDocumentReader();
  ~OpenDocumentReader();

  FileType guess(const std::string &path) const noexcept;

  bool open(const std::string &path) noexcept;
  bool save(const std::string &path) noexcept;
  void close() noexcept;

  bool canTranslate() const noexcept;
  bool canBackTranslate() const noexcept;
  const FileMeta &getMeta() const noexcept;

  bool decrypt(const std::string &password) noexcept;

  bool translate(const std::string &path, const Config &config) noexcept;
  bool backTranslate(const std::string &diff, const std::string &path) noexcept;

private:
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

} // namespace odr

#endif // ODR_OPENDOCUMENTREADER_H
