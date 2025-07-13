#include <odr/internal/cfb/cfb_archive.hpp>

#include <odr/exceptions.hpp>
#include <odr/internal/cfb/cfb_util.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/util/string_util.hpp>

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
  auto filesystem = std::make_shared<common::VirtualFilesystem>();

  for (const auto &e : *m_cfb) {
    if (e.is_directory()) {
      filesystem->create_directory(e.path());
    } else if (e.is_file()) {
      filesystem->copy(e.file(), e.path());
    }
  }

  return filesystem;
}

void CfbArchive::save(std::ostream &out) const {
  (void)out;
  throw UnsupportedOperation();
}

} // namespace odr::internal::cfb
