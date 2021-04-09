#ifndef ODR_INTERNAL_OOXML_DOCUMENT_H
#define ODR_INTERNAL_OOXML_DOCUMENT_H

#include <internal/abstract/document.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlDocument : public virtual abstract::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] ElementIdentifier root_element() const final;
  [[nodiscard]] ElementIdentifier first_entry_element() const final;

  [[nodiscard]] ElementType
  element_type(ElementIdentifier element_id) const final;

  [[nodiscard]] ElementIdentifier
  element_parent(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_first_child(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const final;

  void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  [[nodiscard]] std::shared_ptr<abstract::Table>
  table(ElementIdentifier element_id) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_DOCUMENT_H
