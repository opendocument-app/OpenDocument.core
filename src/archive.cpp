#include <common/archive.h>
#include <odr/archive.h>

namespace odr {

Archive::Archive() = default;

Archive::Archive(std::shared_ptr<common::Archive> impl)
    : m_impl{std::move(impl)} {}

ArchiveEntry::ArchiveEntry() = default;

ArchiveEntry::ArchiveEntry(std::shared_ptr<common::ArchiveEntry> impl)
    : m_impl{std::move(impl)} {}

ArchiveFileEntry::ArchiveFileEntry() = default;

ArchiveFileEntry::ArchiveFileEntry(
    std::shared_ptr<common::ArchiveFileEntry> impl)
    : m_impl{std::move(impl)} {}

ArchiveDirectoryEntry::ArchiveDirectoryEntry() = default;

ArchiveDirectoryEntry::ArchiveDirectoryEntry(
    std::shared_ptr<common::ArchiveDirectoryEntry> impl)
    : m_impl{std::move(impl)} {}

} // namespace odr
