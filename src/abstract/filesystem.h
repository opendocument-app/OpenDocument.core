#ifndef ODR_ABSTRACT_FILESYSTEM_H
#define ODR_ABSTRACT_FILESYSTEM_H

#include <memory>

namespace odr::common {
class Path;
} // namespace odr::common

namespace odr::abstract {
class File;

class FileWalker {
public:
  virtual ~FileWalker() = default;

  [[nodiscard]] virtual std::unique_ptr<FileWalker> clone() const = 0;
  [[nodiscard]] virtual bool equals(const FileWalker &rhs) const = 0;

  [[nodiscard]] virtual common::Path path() const = 0;
  [[nodiscard]] virtual bool is_file() const = 0;
  [[nodiscard]] virtual bool is_directory() const = 0;

  [[nodiscard]] virtual bool has_parent() const = 0;
  [[nodiscard]] virtual bool has_child() const = 0;
  [[nodiscard]] virtual bool has_previous_sibling() const = 0;
  [[nodiscard]] virtual bool has_next_sibling() const = 0;

  virtual void parent() = 0;
  virtual void first_child() = 0;
  virtual void previous_sibling() = 0;
  virtual void next_sibling() = 0;
};

class ReadableFilesystem {
public:
  virtual ~ReadableFilesystem() = default;

  [[nodiscard]] virtual bool exists(common::Path path) const = 0;
  [[nodiscard]] virtual bool is_file(common::Path path) const = 0;
  [[nodiscard]] virtual bool is_directory(common::Path path) const = 0;

  [[nodiscard]] virtual std::unique_ptr<FileWalker>
  file_walker(common::Path path) const = 0;

  [[nodiscard]] virtual std::shared_ptr<abstract::File>
  open(common::Path path) const = 0;
};

class WriteableFilesystem {
public:
  virtual ~WriteableFilesystem() = default;

  virtual std::unique_ptr<std::ostream> create_file(common::Path path) = 0;
  virtual bool create_directory(common::Path path) = 0;

  virtual bool remove(common::Path path) = 0;
  virtual bool copy(common::Path from, common::Path to) = 0;
  virtual std::shared_ptr<abstract::File> copy(abstract::File &from,
                                               common::Path to) = 0;
  virtual std::shared_ptr<abstract::File>
  copy(std::shared_ptr<abstract::File> from, common::Path to) = 0;
  virtual bool move(common::Path from, common::Path to) = 0;
};

class Filesystem : public ReadableFilesystem, public WriteableFilesystem {};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_FILESYSTEM_H