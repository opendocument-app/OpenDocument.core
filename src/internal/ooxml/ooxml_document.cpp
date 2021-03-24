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

const char *OfficeOpenXmlDocument::element_string_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  return ""; // TODO
}

std::uint32_t OfficeOpenXmlDocument::element_uint32_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  return 0; // TODO
}

bool OfficeOpenXmlDocument::element_bool_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  return false; // TODO
}

const char *OfficeOpenXmlDocument::element_optional_string_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  return ""; // TODO
}

TableDimensions
OfficeOpenXmlDocument::table_dimensions(const ElementIdentifier element_id,
                                        const std::uint32_t limit_rows,
                                        const std::uint32_t limit_cols) const {
  return {}; // TODO
}

std::shared_ptr<abstract::File>
OfficeOpenXmlDocument::image_file(const ElementIdentifier element_id) const {
  return {}; // TODO
}

void OfficeOpenXmlDocument::set_element_string_property(
    const ElementIdentifier element_id, const ElementProperty property,
    const char *value) const {
  // TODO
}

void OfficeOpenXmlDocument::remove_element_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  // TODO
}

} // namespace odr::internal::ooxml
