#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <pugixml.hpp>

#include <memory>

namespace odr::internal::odf {

class Document final : public internal::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);
  /// A flat document: one tree holding both content and styles, and no
  /// filesystem behind it.
  Document(FileType file_type, DocumentType document_type,
           pugi::xml_document flat_xml);

  ElementRegistry &element_registry();
  StyleRegistry &style_registry();

  [[nodiscard]] const ElementRegistry &element_registry() const;
  [[nodiscard]] const StyleRegistry &style_registry() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  void init_(pugi::xml_node content_root, pugi::xml_node styles_root);

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::odf
