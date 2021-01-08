#include <abstract/document.h>
#include <abstract/file.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::abstract {

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

FileCategory TextFile::fileCategory() const noexcept {
  return FileCategory::TEXT;
}

FileCategory ImageFile::fileCategory() const noexcept {
  return FileCategory::IMAGE;
}

FileCategory ArchiveFile::fileCategory() const noexcept {
  return FileCategory::ARCHIVE;
}

DocumentType DocumentFile::documentType() const {
  return document()->documentType();
}

DocumentMeta DocumentFile::documentMeta() const {
  return document()->documentMeta();
}

} // namespace odr::abstract
