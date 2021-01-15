#include <abstract/archive.h>
#include <common/filesystem.h>

namespace odr::common {

ArchiveFilesystem::ArchiveFilesystem(std::shared_ptr<abstract::Archive> archive)
    : m_archive{std::move(archive)} {}

} // namespace odr::common
