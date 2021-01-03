#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <common/path.h>
#include <memory>

namespace odr::common {
class Path;
class File;
class ArchiveFile;

class ArchiveEntry;
class ArchiveFileEntry;
class ArchiveDirectoryEntry;

enum class ArchiveEntryType {
  UNKNOWN,
  FILE,
  DIRECTORY,
};

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> first() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry>
  find(const common::Path &path) const = 0;

  [[nodiscard]] virtual std::shared_ptr<ArchiveFileEntry>
  create_default_file(const common::Path &path,
                      std::shared_ptr<File> file) const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveDirectoryEntry>
  create_default_directory(const common::Path &path) const = 0;

  virtual std::shared_ptr<ArchiveEntry>
  append(std::shared_ptr<ArchiveEntry> entry) = 0;
  virtual void rename(std::shared_ptr<ArchiveEntry> entry,
                      const common::Path &path) const = 0;
  virtual void remove(std::shared_ptr<ArchiveEntry> entry) = 0;

  virtual void save(const common::Path &path) const = 0;
};

class ArchiveEntry {
public:
  virtual ~ArchiveEntry() = default;

  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry>
  previous_entry() const = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> next_entry() const = 0;

  virtual std::shared_ptr<ArchiveEntry>
  prepend(std::shared_ptr<ArchiveEntry> entry) = 0;
  virtual std::shared_ptr<ArchiveEntry>
  append(std::shared_ptr<ArchiveEntry> entry) = 0;

  [[nodiscard]] virtual ArchiveEntryType type() const = 0;
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
};

class DefaultArchive : public Archive {
public:
  [[nodiscard]] std::shared_ptr<ArchiveEntry> first() const override;
  [[nodiscard]] std::shared_ptr<ArchiveEntry>
  find(const common::Path &path) const override;

private:
  std::shared_ptr<ArchiveEntry> m_first;
};

class DefaultArchiveEntry : public virtual ArchiveEntry {
public:
  explicit DefaultArchiveEntry(common::Path path);

  [[nodiscard]] std::shared_ptr<ArchiveEntry> previous_entry() const override;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> next_entry() const override;

  std::shared_ptr<ArchiveEntry>
  prepend(std::shared_ptr<ArchiveEntry> entry) override;
  std::shared_ptr<ArchiveEntry>
  append(std::shared_ptr<ArchiveEntry> entry) override;

  [[nodiscard]] common::Path path() const override;

protected:
  common::Path m_path;

  std::weak_ptr<ArchiveEntry> m_previous;
  std::shared_ptr<ArchiveEntry> m_next;
};

} // namespace odr::common

#endif // ODR_COMMON_ARCHIVE_H
