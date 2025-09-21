#pragma once

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <memory>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

class Document final : public TemplateDocument<Element> {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

protected:
  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  StyleRegistry m_style_registry;

  friend class Element;
};

} // namespace odr::internal::odf
