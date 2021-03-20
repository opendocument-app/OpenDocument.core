#include <internal/abstract/document.h>
#include <internal/abstract/file.h>
#include <odr/experimental/document_meta.h>
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

FileCategory DocumentFile::file_category() const noexcept {
  return FileCategory::DOCUMENT;
}

experimental::DocumentType DocumentFile::document_type() const {
  return document()->document_type();
}

experimental::DocumentMeta DocumentFile::document_meta() const {
  return document()->document_meta();
}

} // namespace odr::internal::abstract
