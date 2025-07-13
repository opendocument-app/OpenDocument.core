#pragma once

#include <cstdint>
#include <memory>

namespace odr::internal {
class AbsPath;
} // namespace odr::internal

namespace odr::internal::abstract {
class File;

class FileWalker {
public:
  virtual ~FileWalker() = default;

  [[nodiscard]] virtual std::unique_ptr<FileWalker> clone() const = 0;
  [[nodiscard]] virtual bool equals(const FileWalker &rhs) const = 0;

  [[nodiscard]] virtual bool end() const = 0;
  [[nodiscard]] virtual std::uint32_t depth() const = 0;
  // TODO by reference?
  [[nodiscard]] virtual AbsPath path() const = 0;
  [[nodiscard]] virtual bool is_file() const = 0;
  [[nodiscard]] virtual bool is_directory() const = 0;

  virtual void pop() = 0;
  virtual void next() = 0;
  virtual void flat_next() = 0;
};

class ReadableFilesystem {
public:
  virtual ~ReadableFilesystem() = default;

  [[nodiscard]] virtual bool exists(const AbsPath &path) const = 0;
  [[nodiscard]] virtual bool is_file(const AbsPath &path) const = 0;
  [[nodiscard]] virtual bool is_directory(const AbsPath &path) const = 0;

  [[nodiscard]] virtual std::unique_ptr<FileWalker>
  file_walker(const AbsPath &path) const = 0;

  [[nodiscard]] virtual std::shared_ptr<abstract::File>
  open(const AbsPath &path) const = 0;
};

class WriteableFilesystem {
public:
  virtual ~WriteableFilesystem() = default;

  virtual std::unique_ptr<std::ostream> create_file(const AbsPath &path) = 0;
  virtual bool create_directory(const AbsPath &path) = 0;

  virtual bool remove(const AbsPath &path) = 0;
  virtual bool copy(const AbsPath &from, const AbsPath &to) = 0;
  virtual std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                               const AbsPath &to) = 0;
  virtual std::shared_ptr<abstract::File>
  copy(std::shared_ptr<abstract::File> from, const AbsPath &to) = 0;
  virtual bool move(const AbsPath &from, const AbsPath &to) = 0;
};

class Filesystem : public ReadableFilesystem, public WriteableFilesystem {};

} // namespace odr::internal::abstract
