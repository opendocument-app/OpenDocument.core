#ifndef ODR_FILENOEXCEPT_H
#define ODR_FILENOEXCEPT_H

#include <optional>
#include <memory>
#include <odr/File.h>

namespace odr {

class FileNoExcept {
public:
  static std::optional<FileNoExcept> open(const std::string &path) noexcept;
  static std::optional<FileNoExcept> open(const std::string &path,
                                            FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit FileNoExcept(File &&);
  explicit FileNoExcept(std::unique_ptr<File>);
  FileNoExcept(FileNoExcept &&) noexcept;

  FileType fileType() const noexcept;
  FileCategory fileCategory() const noexcept;
  const FileMeta &fileMeta() const noexcept;

  DocumentNoExcept document() && noexcept;

protected:
  std::unique_ptr<File> m_impl;
};

}

#endif // ODR_FILENOEXCEPT_H
