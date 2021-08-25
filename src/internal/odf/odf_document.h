#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/odf/odf_element.h>
#include <internal/odf/odf_style.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

class OpenDocument : public abstract::Document {
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

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
  root_element() const final;

protected:
  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  Style m_style;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
