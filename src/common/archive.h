#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <abstract/archive.h>
#include <common/path.h>
#include <memory>
#include <vector>

namespace odr::common {

class DefaultArchiveEntry;
class DefaultArchiveEntryIterator;

class DefaultArchive : public abstract::Archive,
                       public std::enable_shared_from_this<DefaultArchive> {
public:
  [[nodiscard]] std::unique_ptr<abstract::ArchiveEntryIterator>
  begin() const override;
  [[nodiscard]] std::unique_ptr<abstract::ArchiveEntryIterator>
  end() const override;

  [[nodiscard]] std::unique_ptr<abstract::ArchiveEntryIterator>
  find(const common::Path &path) const override;
  std::unique_ptr<abstract::ArchiveEntryIterator>
  insert_file(std::unique_ptr<abstract::ArchiveEntryIterator> at,
              common::Path path, std::shared_ptr<abstract::File> file) override;
  std::unique_ptr<abstract::ArchiveEntryIterator>
  insert_directory(std::unique_ptr<abstract::ArchiveEntryIterator> at,
                   common::Path path) override;
  void move(std::shared_ptr<abstract::ArchiveEntry> entry,
            const common::Path &path) const override;
  void remove(std::shared_ptr<abstract::ArchiveEntry> entry) override;

protected:
  std::vector<std::shared_ptr<DefaultArchiveEntry>> m_entries;

  virtual std::unique_ptr<abstract::ArchiveEntryIterator>
  insert(std::unique_ptr<abstract::ArchiveEntryIterator> at,
         std::unique_ptr<abstract::ArchiveEntry> entry);
  virtual std::unique_ptr<DefaultArchiveEntryIterator>
  insert(std::unique_ptr<DefaultArchiveEntryIterator> at,
         std::unique_ptr<DefaultArchiveEntry> entry);
};

class DefaultArchiveEntry : public abstract::ArchiveEntry {
public:
  explicit DefaultArchiveEntry(common::Path path);
  DefaultArchiveEntry(common::Path path, std::shared_ptr<abstract::File> file);

  [[nodiscard]] ArchiveEntryType type() const override;
  [[nodiscard]] common::Path path() const override;

  [[nodiscard]] std::shared_ptr<abstract::File> open() const override;

protected:
  common::Path m_path;
  std::shared_ptr<abstract::File> m_file;

  friend DefaultArchive;
};

} // namespace odr::common

#endif // ODR_COMMON_ARCHIVE_H
