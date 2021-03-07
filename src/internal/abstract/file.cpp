#include <internal/abstract/document.h>
#include <internal/abstract/file.h>
#include <odr/experimental/document_meta.h>
#include <odr/experimental/file_category.h>

namespace odr::internal::abstract {

experimental::FileCategory TextFile::file_category() const noexcept {
  return experimental::FileCategory::TEXT;
}

experimental::FileCategory ImageFile::file_category() const noexcept {
  return experimental::FileCategory::IMAGE;
}

experimental::FileCategory ArchiveFile::file_category() const noexcept {
  return experimental::FileCategory::ARCHIVE;
}

experimental::FileCategory DocumentFile::file_category() const noexcept {
  return experimental::FileCategory::DOCUMENT;
}

experimental::DocumentType DocumentFile::document_type() const {
  return document()->document_type();
}

experimental::DocumentMeta DocumentFile::document_meta() const {
  return document()->document_meta();
}

} // namespace odr::internal::abstract
