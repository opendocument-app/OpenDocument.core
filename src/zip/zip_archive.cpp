#include <common/path.h>
#include <zip/zip_archive.h>

namespace odr::zip {

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<const ZipFile> file)
    : m_file{std::move(file)} {}

std::shared_ptr<common::ArchiveEntry> ZipArchive::first() const {
  return m_first;
}

std::shared_ptr<common::ArchiveEntry>
ZipArchive::find(const common::Path &path) const {
  return {}; // TODO
}

void ZipArchive::save(const common::Path &path) const {
  // TODO
}

ZipArchiveFileEntry::ZipArchiveFileEntry(common::Path path,
                                         std::shared_ptr<common::File> file)
    : m_path{std::move(path)}, m_file{std::move(file)} {}

std::uint8_t ZipArchiveFileEntry::compression_level() const {
  return m_compression_level;
}

void ZipArchiveFileEntry::compression_level(
    const std::uint8_t compression_level) {
  m_compression_level = compression_level;
}

std::shared_ptr<common::File> ZipArchiveFileEntry::open() const {
  return m_file;
}

} // namespace odr::zip
