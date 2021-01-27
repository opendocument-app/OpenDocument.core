#ifndef ODR_ABSTRACT_ARCHIVE_H
#define ODR_ABSTRACT_ARCHIVE_H

#include <memory>
#include <vector>

namespace odr {
enum class ArchiveEntryType;
}

namespace odr::common {
class Path;
}

namespace odr::abstract {
class File;

class ArchiveEntry;
class ArchiveIterator;

class Archive {
public:
  virtual ~Archive() = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveIterator> being() const = 0;
  [[nodiscard]] virtual std::unique_ptr<ArchiveIterator> end() const = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveIterator>
  find(common::Path path) const = 0;

  virtual std::unique_ptr<ArchiveIterator>
  insert_file(std::unique_ptr<ArchiveIterator> at, common::Path path,
              std::shared_ptr<File> file) = 0;

  virtual std::unique_ptr<ArchiveIterator>
  insert_directory(std::unique_ptr<ArchiveIterator> at, common::Path path) = 0;

  virtual bool move(common::Path from, common::Path to) = 0;

  virtual bool remove(common::Path path) = 0;
};

class ArchiveEntry {
public:
  virtual ~ArchiveEntry() = default;

  [[nodiscard]] virtual ArchiveEntryType type() const = 0;
  [[nodiscard]] virtual common::Path path() const = 0;
  [[nodiscard]] virtual std::shared_ptr<File> file() const = 0;

  virtual void file(std::shared_ptr<File> file) = 0;
};

class ArchiveIterator {
public:
  virtual ~ArchiveIterator() = default;

  [[nodiscard]] virtual std::unique_ptr<ArchiveIterator> clone() const = 0;
  [[nodiscard]] virtual bool equals(const ArchiveIterator &rhs) const = 0;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> entry() const = 0;

  [[nodiscard]] virtual bool has_next() const = 0;

  virtual void next() = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_ARCHIVE_H
