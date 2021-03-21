#ifndef ODR_INTERNAL_OOXML_DOCUMENT_H
#define ODR_INTERNAL_OOXML_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <memory>
#include <odr/document_meta.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlDocument : public virtual abstract::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  filesystem() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  DocumentMeta m_document_meta;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_DOCUMENT_H
