#include <internal/common/path.h>
#include <internal/ooxml/ooxml_document.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

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

DocumentType OfficeOpenXmlDocument::document_type() const noexcept {
  return DocumentType::UNKNOWN; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::root_element() const {
  return 0; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::first_entry_element() const {
  return 0; // TODO
}

ElementType
OfficeOpenXmlDocument::element_type(const ElementIdentifier element_id) const {
  return ElementType::NONE; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::element_parent(
    const ElementIdentifier element_id) const {
  return 0; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::element_first_child(
    const ElementIdentifier element_id) const {
  return 0; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::element_previous_sibling(
    const ElementIdentifier element_id) const {
  return 0; // TODO
}

ElementIdentifier OfficeOpenXmlDocument::element_next_sibling(
    const ElementIdentifier element_id) const {
  return 0; // TODO
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlDocument::element_properties(ElementIdentifier element_id) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlDocument::update_element_properties(
    ElementIdentifier element_id,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OfficeOpenXmlDocument::table(const ElementIdentifier element_id) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
