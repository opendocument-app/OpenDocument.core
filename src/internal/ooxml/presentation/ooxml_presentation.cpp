#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/presentation/ooxml_presentation.h>
#include <internal/util/property_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

OfficeOpenXmlPresentation::OfficeOpenXmlPresentation(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  auto presentation_xml =
      util::xml::parse(*m_filesystem, "ppt/presentation.xml");

  // TODO root
}

bool OfficeOpenXmlPresentation::editable() const noexcept { return false; }

bool OfficeOpenXmlPresentation::savable(
    const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlPresentation::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlPresentation::save(const common::Path & /*path*/,
                                     const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlPresentation::document_type() const noexcept {
  return DocumentType::PRESENTATION;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlPresentation::files() const noexcept {
  return m_filesystem;
}

odr::Element OfficeOpenXmlPresentation::root_element() const { return m_root; }

} // namespace odr::internal::ooxml
