#ifndef ODR_ARCHIVE_H
#define ODR_ARCHIVE_H

#include <memory>

namespace odr::common {
class Archive;
class ArchiveEntry;
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

} // namespace odr

#endif // ODR_ARCHIVE_H
