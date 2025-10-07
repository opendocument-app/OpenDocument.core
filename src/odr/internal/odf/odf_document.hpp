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

  ElementRegistry &element_registry();
  StyleRegistry &style_registry();

  const ElementRegistry &element_registry() const;
  const StyleRegistry &style_registry() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::odf
