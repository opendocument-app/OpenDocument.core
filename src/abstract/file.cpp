#include <abstract/document.h>
#include <abstract/file.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::abstract {

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
  return document()->document_type();
}

DocumentMeta DocumentFile::document_meta() const {
  return document()->document_meta();
}

} // namespace odr::abstract
