#ifndef ODR_ABSTRACT_ARCHIVE_H
#define ODR_ABSTRACT_ARCHIVE_H

#include <common/path.h>
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
class ArchiveFile;

class ArchiveEntry;
class ArchiveEntryIterator;

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator> begin() const = 0;
  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator> end() const = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator>
  find(const common::Path &path) const = 0;
  virtual std::unique_ptr<ArchiveEntryIterator>
  insert_file(std::unique_ptr<ArchiveEntryIterator> at, common::Path path,
              std::shared_ptr<File> file) = 0;
  virtual std::unique_ptr<ArchiveEntryIterator>
  insert_directory(std::unique_ptr<ArchiveEntryIterator> at,
                   common::Path path) = 0;
  virtual void move(std::shared_ptr<ArchiveEntry> entry,
                    const common::Path &path) const = 0;
  virtual void remove(std::shared_ptr<ArchiveEntry> entry) = 0;

  virtual void save(const common::Path &path) const = 0;
};

class ArchiveEntry {
public:
  virtual ~ArchiveEntry() = default;

  [[nodiscard]] virtual ArchiveEntryType type() const = 0;
  [[nodiscard]] virtual common::Path path() const = 0;

  [[nodiscard]] virtual std::shared_ptr<File> open() const = 0;
};

class ArchiveEntryIterator {
public:
  virtual ~ArchiveEntryIterator() = default;

  virtual bool operator==(const ArchiveEntryIterator &rhs) const = 0;
  virtual bool operator!=(const ArchiveEntryIterator &rhs) const = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator> copy() const = 0;

  virtual void next() = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> entry() const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_ARCHIVE_H
