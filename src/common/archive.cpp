#include <common/archive.h>

namespace odr::common {

ArchiveEntryType ArchiveFileEntry::type() const {
  return ArchiveEntryType::FILE;
}

ArchiveEntryType ArchiveDirectoryEntry::type() const {
  return ArchiveEntryType::DIRECTORY;
}

DefaultArchiveEntry::DefaultArchiveEntry(common::Path path)
    : m_path{std::move(path)} {}

std::shared_ptr<ArchiveEntry> DefaultArchiveEntry::previous_entry() const {
  return m_previous.lock();
}

std::shared_ptr<ArchiveEntry> DefaultArchiveEntry::next_entry() const {
  return m_next;
}

common::Path DefaultArchiveEntry::path() const { return m_path; }

} // namespace odr::common
