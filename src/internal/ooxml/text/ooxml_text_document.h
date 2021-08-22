#ifndef ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
#define ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <odr/element.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

/*
 * TODO naming - text? word? wordprocessing?
 */
class OfficeOpenXmlTextDocument final : public abstract::Document {
public:
  explicit OfficeOpenXmlTextDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] odr::Element root_element() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  OfficeOpenXmlTextStyle m_style;

  odr::Element m_root;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
