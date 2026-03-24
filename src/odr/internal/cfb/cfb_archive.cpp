#include <odr/internal/cfb/cfb_archive.hpp>

#include <odr/exceptions.hpp>
#include <odr/internal/cfb/cfb_util.hpp>
#include <odr/internal/common/filesystem.hpp>

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class MemoryFile;
} // namespace odr::internal::common

namespace odr::internal::cfb {

CfbArchive::CfbArchive(std::shared_ptr<util::Archive> archive)
    : m_cfb{std::move(archive)} {}

std::shared_ptr<abstract::Filesystem> CfbArchive::as_filesystem() const {
  // TODO return an actual filesystem view
  auto filesystem = std::make_shared<VirtualFilesystem>();

  for (const auto &e : *m_cfb) {
    const AbsPath path = Path(e.path()).make_absolute();

    if (e.is_directory()) {
      filesystem->create_directory(path);
    } else if (e.is_file()) {
      filesystem->copy(e.file(), path);
    }
  }

  return filesystem;
}

void CfbArchive::save([[maybe_unused]] std::ostream &out) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::cfb
