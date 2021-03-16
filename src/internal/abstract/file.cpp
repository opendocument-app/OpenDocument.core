#include <internal/abstract/file.h>
#include <odr/file_category.h>

namespace odr::internal::abstract {

FileCategory TextFile::file_category() const noexcept {
  return FileCategory::TEXT;
}

FileCategory ImageFile::file_category() const noexcept {
  return FileCategory::IMAGE;
}

FileCategory ArchiveFile::file_category() const noexcept {
  return FileCategory::ARCHIVE;
}

} // namespace odr::internal::abstract
