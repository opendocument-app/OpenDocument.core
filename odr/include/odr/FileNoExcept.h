#ifndef ODR_FILENOEXCEPT_H
#define ODR_FILENOEXCEPT_H

#include <odr/File.h>

namespace odr {

class FileNoExcept final {
  static std::unique_ptr<FileNoExcept> open(const std::string &path) noexcept;
  static std::unique_ptr<FileNoExcept> open(const std::string &path,
                                            FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit FileNoExcept(std::unique_ptr<File>);
  FileNoExcept(FileNoExcept &&) noexcept;
  ~FileNoExcept();

  FileType type() const noexcept;
  const FileMeta &meta() const noexcept;

  DocumentNoExcept document() const noexcept;

private:
  std::unique_ptr<File> m_impl;
};

}

#endif // ODR_FILENOEXCEPT_H
