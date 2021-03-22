#include <internal/common/path.h>
#include <internal/ooxml/ooxml_document.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>

namespace odr::internal::ooxml {

OfficeOpenXmlDocument::OfficeOpenXmlDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {}

bool OfficeOpenXmlDocument::editable() const noexcept { return true; }

bool OfficeOpenXmlDocument::savable(bool encrypted) const noexcept {
  return false;
}

void OfficeOpenXmlDocument::save(const common::Path &path) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlDocument::save(const common::Path &path,
                                 const char *password) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
