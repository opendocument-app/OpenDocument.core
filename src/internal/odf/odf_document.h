#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/common/document.h>
#include <internal/odf/odf_element.h>
#include <internal/odf/odf_style.h>
#include <pugixml.hpp>

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

private:
  friend class Table;

  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  DenseElementAttributeStore<odf::Element> m_odf_elements;

  Style m_style;

  ElementIdentifier register_element_(odf::Element element,
                                      ElementIdentifier parent,
                                      ElementIdentifier previous_sibling);
  std::pair<ElementIdentifier, ElementIdentifier>
  register_children_(odf::Element element, ElementIdentifier parent,
                     ElementIdentifier previous_sibling);
  void register_table_children_(odf::TableElement element,
                                ElementIdentifier parent);
  void register_slide_children_(odf::Element element, ElementIdentifier parent);

  ElementIdentifier new_element_(odf::Element element, ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
