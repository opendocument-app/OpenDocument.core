#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>

#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  pugi::xml_document m_document_xml;
  std::unordered_map<std::string, pugi::xml_document> m_slides_xml;

  ElementRegistry m_element_registry;
};

} // namespace odr::internal::ooxml::presentation
