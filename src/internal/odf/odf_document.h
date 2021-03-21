#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/odf/odf_style.h>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::odf {

class OpenDocument : public virtual abstract::Document,
                     public std::enable_shared_from_this<OpenDocument> {
public:
  explicit OpenDocument(std::shared_ptr<abstract::ReadableFilesystem> files);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  std::shared_ptr<abstract::ReadableFilesystem> filesystem() const noexcept;
  const Styles &styles() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_files;
  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;
  Styles m_styles;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
