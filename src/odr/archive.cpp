#include <odr/archive.hpp>

#include <odr/filesystem.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

namespace odr {

Archive::Archive(std::shared_ptr<internal::abstract::Archive> impl)
    : m_impl{std::move(impl)} {}

Archive::operator bool() const { return m_impl.operator bool(); }

Filesystem Archive::filesystem() const {
  return Filesystem(
      std::dynamic_pointer_cast<internal::abstract::ReadableFilesystem>(
          m_impl->filesystem()));
}

void Archive::save(const std::string &path) const { m_impl->save(path); }

} // namespace odr
