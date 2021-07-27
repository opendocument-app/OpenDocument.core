#ifndef ODR_INTERNAL_OOXML_PRESENTATION_H
#define ODR_INTERNAL_OOXML_PRESENTATION_H

#include <internal/common/document.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlPresentation final : public common::Document {
public:
  explicit OfficeOpenXmlPresentation(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const final;

  void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  [[nodiscard]] std::shared_ptr<abstract::Table>
  table(ElementIdentifier element_id) const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  std::vector<pugi::xml_document> m_slides_xml;

  std::unordered_map<ElementIdentifier, pugi::xml_node> m_element_nodes;

  std::any m_slide_width;
  std::any m_slide_height;

  ElementIdentifier register_element_(pugi::xml_node node,
                                      ElementIdentifier parent,
                                      ElementIdentifier previous_sibling);
  std::pair<ElementIdentifier, ElementIdentifier>
  register_children_(pugi::xml_node node, ElementIdentifier parent,
                     ElementIdentifier previous_sibling);

  ElementIdentifier new_element_(pugi::xml_node node, ElementType type,
                                 ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
  pugi::xml_node element_node_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_PRESENTATION_H
