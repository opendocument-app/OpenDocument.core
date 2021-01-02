#ifndef ODR_ZIP_ARCHIVE_H
#define ODR_ZIP_ARCHIVE_H

#include <common/archive.h>
#include <miniz.h>

namespace odr::zip {
class ZipFile;
class ZipArchiveEntry;

class ZipArchive final : public common::Archive {
public:
  ZipArchive();
  explicit ZipArchive(std::shared_ptr<const ZipFile> file);

  [[nodiscard]] bool is_something(const access::Path &) const final;
  [[nodiscard]] bool is_file(const access::Path &) const final;
  [[nodiscard]] bool is_directory(const access::Path &) const final;
  [[nodiscard]] bool is_readable(const access::Path &) const final;
  [[nodiscard]] bool is_writeable(const access::Path &) const final;

  [[nodiscard]] std::shared_ptr<common::ArchiveEntry> root() const final;
  [[nodiscard]] std::shared_ptr<common::ArchiveEntry>
  entry(const access::Path &) const final;

  [[nodiscard]] std::shared_ptr<common::File>
  open(const access::Path &) const final;

  [[nodiscard]] bool remove(const access::Path &) const final;
  [[nodiscard]] bool copy(const access::Path &from,
                          const access::Path &to) const final;
  [[nodiscard]] bool copy(const std::shared_ptr<common::File> &file,
                          const access::Path &to) const final;
  [[nodiscard]] bool move(const access::Path &from,
                          const access::Path &to) const final;

  [[nodiscard]] bool create_directory(const access::Path &) const final;
  [[nodiscard]] std::unique_ptr<std::ostream>
  create_file(const access::Path &) const final;

private:
  std::shared_ptr<const ZipFile> m_file;
};

class ZipArchiveEntry final : public common::ArchiveEntry {
public:
  [[nodiscard]] std::shared_ptr<ArchiveEntry> parent() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> first_child() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> previous() const final;
  [[nodiscard]] std::shared_ptr<ArchiveEntry> next() const final;

  [[nodiscard]] access::Path path() const final;
  [[nodiscard]] std::shared_ptr<common::File> open() const final;

private:
};

} // namespace odr::zip

#endif // ODR_ZIP_ARCHIVE_H
