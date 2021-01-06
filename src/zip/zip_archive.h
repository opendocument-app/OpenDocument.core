#ifndef ODR_ZIP_ARCHIVE_H
#define ODR_ZIP_ARCHIVE_H

#include <common/archive.h>
#include <common/path.h>
#include <miniz.h>
#include <vector>

namespace odr::zip {
class ZipFile;

class ZipArchive final : public common::Archive,
                         public std::enable_shared_from_this<ZipArchive> {
public:
  ZipArchive();
  explicit ZipArchive(std::shared_ptr<const ZipFile> file);

  [[nodiscard]] std::shared_ptr<common::ArchiveEntry> first() const final;
  [[nodiscard]] std::shared_ptr<common::ArchiveEntry>
  find(const common::Path &path) const final;

  [[nodiscard]] std::shared_ptr<common::ArchiveEntry>
  create_default_file(const common::Path &path,
                      std::shared_ptr<common::File> file) const final;
  [[nodiscard]] std::shared_ptr<common::ArchiveEntry>
  create_default_directory(const common::Path &path) const final;

  std::shared_ptr<common::ArchiveEntry>
  append(std::shared_ptr<common::ArchiveEntry> entry) final;
  void rename(std::shared_ptr<common::ArchiveEntry> entry,
              const common::Path &path) const final;
  void remove(std::shared_ptr<common::ArchiveEntry> entry) final;

  void save(const common::Path &path) const final;

private:
  std::shared_ptr<const ZipFile> m_file;
  std::shared_ptr<common::ArchiveEntry> m_first;
};

class ZipArchiveEntry final : public common::ArchiveEntry {
public:
  ZipArchiveEntry(common::Path path, std::shared_ptr<common::File> file);

  [[nodiscard]] std::uint8_t compression_level() const;
  void compression_level(std::uint8_t);

  [[nodiscard]] std::shared_ptr<common::File> open() const final;

private:
  common::Path m_path;
  std::shared_ptr<common::File> m_file;
  std::uint8_t m_compression_level{6};
};

} // namespace odr::zip

#endif // ODR_ZIP_ARCHIVE_H
