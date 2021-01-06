#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <common/path.h>
#include <memory>
#include <vector>

namespace odr {
enum class ArchiveEntryType;
}

namespace odr::common {
class Path;
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

  virtual void next() = 0;
  [[nodiscard]] virtual std::shared_ptr<ArchiveEntry> entry() const = 0;
};

class DefaultArchiveEntry;
class DefaultArchiveEntryIterator;

class DefaultArchive : public Archive,
                       public std::enable_shared_from_this<DefaultArchive> {
public:
  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator> begin() const override;
  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator> end() const override;

  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator>
  find(const common::Path &path) const override;
  std::unique_ptr<ArchiveEntryIterator>
  insert_file(std::unique_ptr<ArchiveEntryIterator> at, common::Path path,
              std::shared_ptr<File> file) override;
  std::unique_ptr<ArchiveEntryIterator>
  insert_directory(std::unique_ptr<ArchiveEntryIterator> at,
                   common::Path path) override;
  void move(std::shared_ptr<ArchiveEntry> entry,
            const common::Path &path) const override;
  void remove(std::shared_ptr<ArchiveEntry> entry) override;

protected:
  std::vector<std::shared_ptr<DefaultArchiveEntry>> m_entries;

  virtual std::unique_ptr<ArchiveEntryIterator>
  insert(std::unique_ptr<ArchiveEntryIterator> at,
         std::unique_ptr<ArchiveEntry> entry);
  virtual std::unique_ptr<DefaultArchiveEntryIterator>
  insert(std::unique_ptr<DefaultArchiveEntryIterator> at,
         std::unique_ptr<DefaultArchiveEntry> entry);
};

class DefaultArchiveEntry : public virtual ArchiveEntry {
public:
  explicit DefaultArchiveEntry(common::Path path);
  DefaultArchiveEntry(common::Path path, std::shared_ptr<File> file);

  [[nodiscard]] ArchiveEntryType type() const override;
  [[nodiscard]] common::Path path() const override;

  [[nodiscard]] std::shared_ptr<File> open() const override;

protected:
  common::Path m_path;
  std::shared_ptr<File> m_file;

  friend DefaultArchive;
};

} // namespace odr::common

#endif // ODR_COMMON_ARCHIVE_H
