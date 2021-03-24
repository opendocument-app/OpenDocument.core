#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/abstract/document.h>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>
#include <vector>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::odf {

class OpenDocument final : public abstract::Document {
public:
  OpenDocument(DocumentType document_type,
               std::shared_ptr<abstract::ReadableFilesystem> files);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

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

  [[nodiscard]] const char *
  element_string_property(ElementIdentifier element_id,
                          ElementProperty property) const final;
  [[nodiscard]] std::uint32_t
  element_uint32_property(ElementIdentifier element_id,
                          ElementProperty property) const final;
  [[nodiscard]] bool
  element_bool_property(ElementIdentifier element_id,
                        ElementProperty property) const final;
  [[nodiscard]] const char *
  element_optional_string_property(ElementIdentifier element_id,
                                   ElementProperty property) const final;

  [[nodiscard]] TableDimensions
  table_dimensions(ElementIdentifier element_id, std::uint32_t limit_rows,
                   std::uint32_t limit_cols) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  image_file(ElementIdentifier element_id) const final;

  void set_element_string_property(ElementIdentifier element_id,
                                   ElementProperty property,
                                   const char *value) const final;

  void remove_element_property(ElementIdentifier element_id,
                               ElementProperty property) const final;

private:
  struct Element {
    pugi::xml_node node;
    ElementType type{ElementType::NONE};
    ElementIdentifier parent;
    ElementIdentifier first_child;
    ElementIdentifier previous_sibling;
    ElementIdentifier next_sibling;
  };

  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  std::vector<Element> m_elements;
  ElementIdentifier m_root;

  ElementIdentifier register_tree_(pugi::xml_node node,
                                   ElementIdentifier parent,
                                   ElementIdentifier previous_sibling);

  ElementIdentifier register_element_(const Element &element);
  Element *element_(ElementIdentifier element_id);
  const Element *element_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
