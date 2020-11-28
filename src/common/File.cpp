#include <common/File.h>
#include <common/Document.h>
#include <odr/Document.h>

namespace odr::common {

DocumentType DocumentFile::documentType() const noexcept {
  return document()->documentType();
}

DocumentMeta DocumentFile::documentMeta() const noexcept {
  return document()->documentMeta();
}

}
