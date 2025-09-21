#pragma once

#include <memory>
#include <string>

namespace odr::internal::abstract {
class FileWalker;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr {
class File;

/// @brief FileWalker class
class FileWalker {
public:
  FileWalker();
  explicit FileWalker(std::unique_ptr<internal::abstract::FileWalker>);
  FileWalker(const FileWalker &);
  FileWalker(FileWalker &&) noexcept;
  ~FileWalker();

  FileWalker &operator=(const FileWalker &);
  FileWalker &operator=(FileWalker &&) noexcept;
  [[nodiscard]] explicit operator bool() const;

  [[nodiscard]] bool end() const;
  [[nodiscard]] std::uint32_t depth() const;
  [[nodiscard]] std::string path() const;
  [[nodiscard]] bool is_file() const;
  [[nodiscard]] bool is_directory() const;

  void pop() const;
  void next() const;
  void flat_next() const;

private:
  std::unique_ptr<internal::abstract::FileWalker> m_impl;
};

/// @brief Filesystem class
class Filesystem {
public:
  explicit Filesystem(std::shared_ptr<internal::abstract::ReadableFilesystem>);

  [[nodiscard]] explicit operator bool() const;

  [[nodiscard]] bool exists(const std::string &path) const;
  [[nodiscard]] bool is_file(const std::string &path) const;
  [[nodiscard]] bool is_directory(const std::string &path) const;

  [[nodiscard]] FileWalker file_walker(const std::string &path) const;

  [[nodiscard]] File open(const std::string &path) const;

private:
  std::shared_ptr<internal::abstract::ReadableFilesystem> m_impl;
};

} // namespace odr
