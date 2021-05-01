#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/odf/odf_style.h>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class ReadableFilesystem;
class Table;
} // namespace odr::internal::abstract

namespace odr::internal::odf {
class Table;

class OpenDocument final : public abstract::Document,
                           public std::enable_shared_from_this<OpenDocument> {
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

private:
  friend class Table;

  class PropertyRegistry;

  struct Element {
    pugi::xml_node node;
    ElementType type{ElementType::NONE};

    ElementIdentifier parent;
    ElementIdentifier first_child;
    ElementIdentifier previous_sibling;
    ElementIdentifier next_sibling;
  };

  struct TableElementExtension {
    std::uint32_t row;
    std::uint32_t column;
  };

  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  std::vector<Element> m_elements;
  ElementIdentifier m_root;

  std::unordered_map<ElementIdentifier, std::shared_ptr<Table>> m_tables;
  std::unordered_map<ElementIdentifier, TableElementExtension>
      m_table_element_extension;

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
  Element *element_(ElementIdentifier element_id);
  const Element *element_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
