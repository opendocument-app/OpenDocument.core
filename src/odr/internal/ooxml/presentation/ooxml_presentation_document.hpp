#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;
  /// The scheme of the slide @p element_id, or null where it relates no master.
  [[nodiscard]] const ColorScheme *
  slide_color_scheme(ElementIdentifier element_id) const;
  /// The shared slide layout, plus the ground this slide states or inherits.
  [[nodiscard]] PageLayout
  slide_page_layout(ElementIdentifier element_id) const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  pugi::xml_document m_document_xml;
  std::unordered_map<std::string, pugi::xml_document> m_slides_xml;
  PageLayout m_slide_layout;
  /// by slide master path; a slide points into this, so it has to outlive them
  std::unordered_map<std::string, ColorScheme> m_color_schemes;
  std::unordered_map<ElementIdentifier, const ColorScheme *>
      m_slide_color_schemes;
  std::unordered_map<ElementIdentifier, Color> m_slide_backgrounds;

  ElementRegistry m_element_registry;

  void load_slide_styles_(const std::vector<AbsPath> &slides);
};

} // namespace odr::internal::ooxml::presentation
