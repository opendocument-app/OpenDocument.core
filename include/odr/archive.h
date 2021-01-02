#ifndef ODR_ARCHIVE_H
#define ODR_ARCHIVE_H

#include <memory>

namespace odr::common {
class Archive;
class ArchiveEntry;
class ArchiveFileEntry;
class ArchiveDirectoryEntry;
} // namespace odr::common

namespace odr {
class ArchiveEntry;

class Archive {
public:
  Archive();
  explicit Archive(std::shared_ptr<common::Archive> impl);

private:
  std::shared_ptr<common::Archive> m_impl;
};

class ArchiveEntry {
public:
  ArchiveEntry();
  explicit ArchiveEntry(std::shared_ptr<common::ArchiveEntry> impl);

private:
  std::shared_ptr<common::ArchiveEntry> m_impl;
};

class ArchiveFileEntry : public ArchiveEntry {
public:
  ArchiveFileEntry();
  explicit ArchiveFileEntry(std::shared_ptr<common::ArchiveFileEntry> impl);

private:
  std::shared_ptr<common::ArchiveFileEntry> m_impl;
};

class ArchiveDirectoryEntry : public ArchiveEntry {
public:
  ArchiveDirectoryEntry();
  explicit ArchiveDirectoryEntry(
      std::shared_ptr<common::ArchiveDirectoryEntry> impl);

private:
  std::shared_ptr<common::ArchiveDirectoryEntry> m_impl;
};

} // namespace odr

#endif // ODR_ARCHIVE_H
