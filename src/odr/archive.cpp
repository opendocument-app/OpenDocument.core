#include <odr/archive.hpp>

#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

namespace odr {

Archive::Archive(std::shared_ptr<internal::abstract::Archive> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("Archive implementation is null");
  }
}

Filesystem Archive::as_filesystem() const {
  return Filesystem(
      std::dynamic_pointer_cast<internal::abstract::ReadableFilesystem>(
          m_impl->filesystem()));
}

void Archive::save(std::ostream &out) const { m_impl->save(out); }

} // namespace odr
