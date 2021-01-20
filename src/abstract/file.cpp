#include <abstract/document.h>
#include <abstract/file.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::abstract {

FileCategory File::file_category() const noexcept {
  return FileMeta::categoryByType(file_type());
}

FileCategory TextFile::file_category() const noexcept {
  return FileCategory::TEXT;
}

FileCategory ImageFile::file_category() const noexcept {
  return FileCategory::IMAGE;
}

FileCategory ArchiveFile::file_category() const noexcept {
  return FileCategory::ARCHIVE;
}

DocumentType DocumentFile::document_type() const {
  return document()->documentType();
}

DocumentMeta DocumentFile::document_meta() const {
  return document()->documentMeta();
}

} // namespace odr::abstract
