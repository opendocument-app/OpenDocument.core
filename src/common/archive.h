#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <memory>

namespace odr::common {
class Path;
class File;
class ArchiveFile;

class ArchiveEntry;
class ArchiveDirectoryEntry;

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::shared_ptr<ArchiveDirectoryEntry> root() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry>
  entry(const common::Path &) const = 0;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> first() const = 0;

  virtual void save(const common::Path &) const = 0;
};

enum class ArchiveEntryType {
  UNKNOWN,
  FILE,
  DIRECTORY,
};

class ArchiveEntry {
public:
  virtual ~ArchiveEntry() = default;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> parent() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> first_child() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry>
  previous_sibling() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> next_sibling() const = 0;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry>
  previous_entry() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> next_entry() const = 0;

  [[nodiscard]] virtual ArchiveEntryType type() const = 0;
  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual common::Path path() const = 0;
};

class ArchiveFileEntry : public virtual ArchiveEntry {
public:
  [[nodiscard]] ArchiveEntryType type() const final;

  [[nodiscard]] virtual std::shared_ptr<File> open() const = 0;
};

class ArchiveDirectoryEntry : public virtual ArchiveEntry {
public:
  [[nodiscard]] ArchiveEntryType type() const final;

  [[nodiscard]] virtual std::shared_ptr<ArchiveDirectoryEntry>
  create_directory(const std::string &) const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveFileEntry>
  create_file(const std::string &) const = 0;
};

} // namespace odr::common

#endif // ODR_COMMON_ARCHIVE_H
