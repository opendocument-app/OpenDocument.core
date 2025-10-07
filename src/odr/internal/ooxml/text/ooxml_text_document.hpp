#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element_registry.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  std::unordered_map<std::string, std::string> m_document_relations;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::ooxml::text
