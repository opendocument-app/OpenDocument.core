#ifndef ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
#define ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

class Document final : public common::TemplateDocument<Element> {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

private:
  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  std::unordered_map<std::string, std::string> m_document_relations;

  StyleRegistry m_style_registry;

  friend class Element;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
