#ifndef ODR_ABSTRACT_FILESYSTEM_H
#define ODR_ABSTRACT_FILESYSTEM_H

#include <memory>

namespace odr::common {
class File;
class Path;
} // namespace odr::common

namespace odr::abstract {

class FileWalker;

class Filesystem {
public:
  virtual ~Filesystem() = default;

  [[nodiscard]] virtual bool exists(const common::Path &path) const = 0;
  [[nodiscard]] virtual bool is_file(const common::Path &path) const = 0;
  [[nodiscard]] virtual bool is_directory(const common::Path &path) const = 0;

  [[nodiscard]] virtual std::unique_prt<abstract::File>
  file_walker(const common::Path &path) const = 0;

  [[nodiscard]] virtual std::shared_prt<abstract::File>
  open(const common::Path &path) const = 0;

  virtual bool remove(const common::Path &path) const = 0;
  virtual bool copy(const common::Path &from, const common::Path &to) const = 0;
  virtual std::shared_prt<abstract::File>
  copy(abstract::File &from, const common::Path &to) const = 0;
  virtual bool move(const common::Path &from, const common::Path &to) const = 0;

  virtual bool create_directory(const common::Path &path) const = 0;
};

class FileWalker {
public:
  virtual ~FileWalker() = default;

  virtual bool operator==(const FileWalker &rhs) const = 0;
  virtual bool operator!=(const FileWalker &rhs) const = 0;

  [[nodiscard]] virtual std::unique_ptr<FileWalker> copy() const = 0;

  [[nodiscard]] virtual common::Path path() const = 0;
  [[nodiscard]] virtual bool is_file() const = 0;
  [[nodiscard]] virtual bool is_directory() const = 0;

  virtual void parent() = 0;
  virtual void open_directory() = 0;
  virtual void next_sibling() = 0;
}

} // namespace odr::abstract

#endif // ODR_ABSTRACT_FILESYSTEM_H
