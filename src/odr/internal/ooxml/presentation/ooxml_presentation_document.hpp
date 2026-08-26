#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

/// The theme's `a:clrScheme` seen through the slide master's `p:clrMap`, so a
/// slide's `a:schemeClr` names a colour. [ECMA-376] 20.1.6.2
class ColorScheme final {
public:
  ColorScheme() = default;
  ColorScheme(pugi::xml_node color_scheme, pugi::xml_node color_map);

  [[nodiscard]] std::optional<Color> resolve(const char *name) const;

private:
  std::unordered_map<std::string, Color> m_colors;
};

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;
  [[nodiscard]] const PageLayout &slide_layout() const;
  /// The scheme of the slide @p element_id, or null where the chain to a theme
  /// is broken.
  [[nodiscard]] const ColorScheme *
  slide_color_scheme(ElementIdentifier element_id) const;
  /// The layout of the slide @p element_id: the shared one, plus the ground the
  /// slide inherits from its layout or master.
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
  std::unordered_map<ElementIdentifier, ColorScheme> m_slide_color_schemes;
  std::unordered_map<ElementIdentifier, Color> m_slide_backgrounds;

  ElementRegistry m_element_registry;

  void load_slide_styles_(
      const std::vector<std::pair<std::string, AbsPath>> &slides);
};

} // namespace odr::internal::ooxml::presentation
