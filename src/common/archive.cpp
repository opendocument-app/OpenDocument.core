#include <common/archive.h>

namespace odr::common {

ArchiveEntryType ArchiveFileEntry::type() const {
  return ArchiveEntryType::FILE;
}

ArchiveEntryType ArchiveDirectoryEntry::type() const {
  return ArchiveEntryType::DIRECTORY;
}

} // namespace odr::common
