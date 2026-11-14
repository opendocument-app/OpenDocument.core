#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files,
                    std::shared_ptr<zip::util::Archive> archive = nullptr);

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;

  [[nodiscard]] const ElementRegistry &element_registry() const;
  /// The layout, master and theme of the slide @p element_id, or null where
  /// it relates no layout.
  [[nodiscard]] const LayoutStyle *
  slide_style(ElementIdentifier element_id) const;
  /// `p:defaultTextStyle`, the list style every shape starts from.
  [[nodiscard]] pugi::xml_node default_text_style() const;
  /// The shared slide layout, plus the ground this slide states or inherits.
  [[nodiscard]] PageLayout
  slide_page_layout(ElementIdentifier element_id) const;

private:
  pugi::xml_document m_document_xml;
  /// by the `r:id` the slide-id list names, which is how a slide is reached
  std::unordered_map<std::string, pugi::xml_document> m_slides_xml;
  /// the other way round, which is what `save` walks
  std::map<AbsPath, std::string> m_slide_ids_by_path;
  PageLayout m_slide_layout;
  /// by slide master path; a slide points into this, so it has to outlive them
  std::unordered_map<std::string, Theme> m_themes;
  /// the layouts and masters by path, which a slide's style points into
  std::unordered_map<std::string, pugi::xml_document> m_style_parts;
  std::unordered_map<ElementIdentifier, LayoutStyle> m_slide_styles;
  std::unordered_map<ElementIdentifier, Color> m_slide_backgrounds;

  ElementRegistry m_element_registry;

  void load_slide_styles_(const std::vector<AbsPath> &slides);
};

} // namespace odr::internal::ooxml::presentation
