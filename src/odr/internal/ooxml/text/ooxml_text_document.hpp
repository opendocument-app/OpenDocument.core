#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element_registry.hpp>
#include <odr/internal/ooxml/text/ooxml_text_list.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <memory>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

class Document final : public internal::Document {
public:
  Document(std::shared_ptr<abstract::ReadableFilesystem> files,
           EncryptionState encryption_state);

  ElementRegistry &element_registry();
  StyleRegistry &style_registry();

  [[nodiscard]] const ElementRegistry &element_registry() const;
  [[nodiscard]] const StyleRegistry &style_registry() const;
  [[nodiscard]] const Relations &document_relations() const;
  [[nodiscard]] const PageLayout &page_layout() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;

private:
  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;
  pugi::xml_document m_numbering_xml;

  Relations m_document_relations;

  PageLayout m_page_layout;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
  NumberingRegistry m_numbering_registry;
};

} // namespace odr::internal::ooxml::text
