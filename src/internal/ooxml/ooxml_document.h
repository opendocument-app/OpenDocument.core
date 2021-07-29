#ifndef ODR_INTERNAL_OOXML_DOCUMENT_H
#define ODR_INTERNAL_OOXML_DOCUMENT_H

#include <internal/common/document.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlDocument : public common::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  std::unordered_map<ElementIdentifier, pugi::xml_node> m_element_nodes;

  virtual ElementIdentifier
  register_element_(pugi::xml_node node, ElementIdentifier parent,
                    ElementIdentifier previous_sibling);
  std::pair<ElementIdentifier, ElementIdentifier> virtual register_children_(
      pugi::xml_node node, ElementIdentifier parent,
      ElementIdentifier previous_sibling);

  ElementIdentifier new_element_(pugi::xml_node node, ElementType type,
                                 ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
  pugi::xml_node element_node_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_DOCUMENT_H
