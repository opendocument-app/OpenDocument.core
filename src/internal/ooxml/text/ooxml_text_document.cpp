#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/util/property_util.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  m_style = OfficeOpenXmlTextStyle(m_styles_xml.document_element());

  // TODO root
}

bool OfficeOpenXmlTextDocument::editable() const noexcept { return false; }

bool OfficeOpenXmlTextDocument::savable(
    const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlTextDocument::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlTextDocument::save(const common::Path & /*path*/,
                                     const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlTextDocument::document_type() const noexcept {
  return DocumentType::TEXT;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlTextDocument::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor>
OfficeOpenXmlTextDocument::root_element() const {
  return {}; // TODO
}

} // namespace odr::internal::ooxml
