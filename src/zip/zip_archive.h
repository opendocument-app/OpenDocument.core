#ifndef ODR_ZIP_ARCHIVE_H
#define ODR_ZIP_ARCHIVE_H

#include <common/archive.h>
#include <miniz.h>

namespace odr::zip {
class ZipFile;
class ZipArchiveEntry;

class ZipArchive final : public common::Archive,
                         public std::enable_shared_from_this<ZipArchive> {
public:
  ZipArchive();
  explicit ZipArchive(std::shared_ptr<const ZipFile> file);

  [[nodiscard]] std::shared_ptr<common::ArchiveDirectoryEntry>
  root() const final;
  [[nodiscard]] std::shared_ptr<common::ArchiveEntry>
  entry(const common::Path &) const final;

  [[nodiscard]] std::shared_ptr<common::ArchiveEntry> first() const final;

  void save(const common::Path &) const final;

private:
  std::shared_ptr<const ZipFile> m_file;

  std::shared_ptr<common::ArchiveDirectoryEntry> m_root;
  std::shared_ptr<common::ArchiveEntry> m_first;
};

class ZipArchiveEntry : public virtual common::ArchiveEntry {
public:
  ZipArchiveEntry(std::shared_ptr<const ZipArchive> archive, mz_uint index);

  [[nodiscard]] std::shared_ptr<ArchiveEntry> parent() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> first_child() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> previous_sibling() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> next_sibling() const final;

  [[nodiscard]] std::shared_ptr<ArchiveEntry> previous_entry() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> next_entry() const final;

  [[nodiscard]] std::string name() const final;
  [[nodiscard]] common::Path path() const final;

private:
  std::shared_ptr<const ZipArchive> m_archive;
  mz_uint m_index;
};

class ZipArchiveFileEntry final : public common::ArchiveFileEntry,
                                  public ZipArchiveEntry {
public:
  [[nodiscard]] std::shared_ptr<common::File> open() const final;

private:
};

class ZipArchiveDirectoryEntry final : public common::ArchiveDirectoryEntry,
                                       public ZipArchiveEntry {
public:
  [[nodiscard]] std::shared_ptr<common::ArchiveDirectoryEntry>
  create_directory(const std::string &) const final;
  [[nodiscard]] std::shared_ptr<common::ArchiveFileEntry>
  create_file(const std::string &) const final;

private:
};

} // namespace odr::zip

#endif // ODR_ZIP_ARCHIVE_H
