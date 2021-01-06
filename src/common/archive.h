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

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntry>
  create_file(const common::Path &path, std::shared_ptr<File> file) const = 0;
  [[nodiscard]] virtual std::unique_ptr<ArchiveEntry>
  create_directory(const common::Path &path) const = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator> begin() const = 0;
  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator> end() const = 0;

  [[nodiscard]] virtual std::unique_ptr<ArchiveEntryIterator>
  find(const common::Path &path) const = 0;
  virtual std::unique_ptr<ArchiveEntryIterator>
  prepend(std::unique_ptr<ArchiveEntry> entry,
          std::unique_ptr<ArchiveEntryIterator> to) = 0;
  virtual std::unique_ptr<ArchiveEntryIterator>
  append(std::unique_ptr<ArchiveEntry> entry,
         std::unique_ptr<ArchiveEntryIterator> to) = 0;
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

class DefaultArchive : public Archive,
                       public std::enable_shared_from_this<DefaultArchive> {
public:
  [[nodiscard]] std::unique_ptr<ArchiveEntry>
  create_file(const common::Path &path,
              std::shared_ptr<File> file) const override;
  [[nodiscard]] std::unique_ptr<ArchiveEntry>
  create_directory(const common::Path &path) const override;

  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator> begin() const override;
  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator> end() const override;

  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator>
  find(const common::Path &path) const override;
  std::unique_ptr<ArchiveEntryIterator>
  prepend(std::unique_ptr<ArchiveEntry> entry,
          std::unique_ptr<ArchiveEntryIterator> to) override;
  std::unique_ptr<ArchiveEntryIterator>
  append(std::unique_ptr<ArchiveEntry> entry,
         std::unique_ptr<ArchiveEntryIterator> to) override;
  void move(std::shared_ptr<ArchiveEntry> entry,
            const common::Path &path) const override;
  void remove(std::shared_ptr<ArchiveEntry> entry) override;

private:
  std::vector<std::shared_ptr<DefaultArchiveEntry>> m_entries;
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
