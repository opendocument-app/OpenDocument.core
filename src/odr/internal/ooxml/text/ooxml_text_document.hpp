#pragma once

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

class Document final : public TemplateDocument<Element> {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool is_editable() const noexcept final;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept final;

  void save(const Path &path) const final;
  void save(const Path &path, const char *password) const final;

private:
  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  std::unordered_map<std::string, std::string> m_document_relations;

  StyleRegistry m_style_registry;

  friend class Element;
};

} // namespace odr::internal::ooxml::text
