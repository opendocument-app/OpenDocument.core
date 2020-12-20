#include <common/document.h>
#include <common/file.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::common {

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

DocumentType DocumentFile::documentType() const {
  return document()->documentType();
}

DocumentMeta DocumentFile::documentMeta() const {
  return document()->documentMeta();
}

} // namespace odr::common
