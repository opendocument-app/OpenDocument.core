#ifndef ODR_COMMON_FILESYSTEM_H
#define ODR_COMMON_FILESYSTEM_H

#include <abstract/filesystem.h>
#include <memory>

namespace odr::abstract {
class Archive;
}

namespace odr::common {

class ArchiveFilesystem : public abstract::Filesystem {
public:
  explicit ArchiveFilesystem(std::shared_ptr<abstract::Archive> archive);

private:
  std::shared_ptr<abstract::Archive> m_archive;
};

} // namespace odr::common

#endif // ODR_COMMON_FILESYSTEM_H
