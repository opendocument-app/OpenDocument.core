#include <internal/abstract/document.h>
#include <internal/abstract/file.h>
#include <odr/file.h>

namespace odr::internal::abstract {

FileCategory TextFile::file_category() const noexcept {
  return FileCategory::text;
}

FileCategory ImageFile::file_category() const noexcept {
  return FileCategory::image;
}

FileCategory ArchiveFile::file_category() const noexcept {
  return FileCategory::archive;
}

FileCategory DocumentFile::file_category() const noexcept {
  return FileCategory::document;
}

DocumentType DocumentFile::document_type() const {
  return document()->document_type();
}

DocumentMeta DocumentFile::document_meta() const {
  return {}; // TODO
}

} // namespace odr::internal::abstract
