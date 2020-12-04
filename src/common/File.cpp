#include <common/Document.h>
#include <common/File.h>
#include <odr/Document.h>
#include <odr/File.h>

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
