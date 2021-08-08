#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/common/document.h>
#include <internal/odf/odf_style.h>
#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class ReadableFilesystem;
class Table;
} // namespace odr::internal::abstract

namespace odr::internal::odf {
class Table;

class OpenDocument final : public common::Document {
public:
  OpenDocument(DocumentType document_type,
               std::shared_ptr<abstract::ReadableFilesystem> files);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

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

private:
  friend class Table;

  class PropertyRegistry;

  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  std::unordered_map<ElementIdentifier, pugi::xml_node> m_element_nodes;

  Style m_style;

  ElementIdentifier register_element_(pugi::xml_node node,
                                      ElementIdentifier parent,
                                      ElementIdentifier previous_sibling);
  std::pair<ElementIdentifier, ElementIdentifier>
  register_children_(pugi::xml_node node, ElementIdentifier parent,
                     ElementIdentifier previous_sibling);

  void post_register_table_(ElementIdentifier element, pugi::xml_node node);
  void post_register_slide_(ElementIdentifier element, pugi::xml_node node);

  ElementIdentifier new_element_(pugi::xml_node node, ElementType type,
                                 ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
  pugi::xml_node element_node_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
