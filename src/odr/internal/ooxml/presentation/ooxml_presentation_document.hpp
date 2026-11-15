#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element_registry.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
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

  /// A slide outside the root per layout of every master, in
  /// `p:sldLayoutIdLst` order, as @ref insert_slide makes it.
  [[nodiscard]] std::vector<ElementIdentifier> slide_layouts() const;
  /// A text box outside the root as @ref insert_text_box makes it.
  [[nodiscard]] ElementIdentifier text_box_layout() const;
  /// A copy of the @p layout -th of @ref slide_layouts at @p index: a new part
  /// with its relationships and content type, and an entry in `p:sldIdLst`.
  ElementIdentifier insert_slide(std::uint32_t index, std::uint32_t layout);
  /// Takes the slide @p slide_id out: its entry in `p:sldIdLst`, custom shows
  /// and sections, its relationship, content type, part and notes slide.
  void remove_slide(ElementIdentifier slide_id);
  /// Puts the slide @p slide_id at @p index among the slides, counted after
  /// the move, into the section of the slide it then follows.
  void move_slide(ElementIdentifier slide_id, std::uint32_t index);
  /// A `p:sp` text box with one empty paragraph, last on the slide
  /// @p slide_id.
  ElementIdentifier insert_text_box(ElementIdentifier slide_id,
                                    const Measure &x, const Measure &y,
                                    const Measure &width,
                                    const Measure &height);
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
  /// the relationships of `presentation.xml`
  Relations m_relations;
  /// `presentation.xml` changed, so `save` writes it from its dom
  bool m_presentation_changed{false};
  /// the other parts an edit changed or made, which `save` writes
  std::map<AbsPath, pugi::xml_document> m_written_parts;
  /// the parts of the package `save` leaves out
  std::vector<AbsPath> m_dropped_parts;
  /// by the `r:id` the slide-id list names, which is how a slide is reached
  std::unordered_map<std::string, pugi::xml_document> m_slides_xml;
  /// the other way round, which is what `save` walks
  std::map<AbsPath, std::string> m_slide_ids_by_path;
  PageLayout m_slide_layout;
  /// declared before `m_slide_styles`, which points into it
  LayoutStyles m_layout_styles;
  /// what `slide_layouts` and `text_box_layout` hold, outside the package
  std::vector<std::unique_ptr<pugi::xml_document>> m_layouts_xml;
  std::vector<std::pair<ElementIdentifier, AbsPath>> m_slide_layouts;
  ElementIdentifier m_text_box_layout{null_element_id};
  std::unordered_map<ElementIdentifier, const LayoutStyle *> m_slide_styles;
  std::unordered_map<ElementIdentifier, Color> m_slide_backgrounds;

  ElementRegistry m_element_registry;

  void load_slide_styles_(const std::vector<AbsPath> &slides);
  void load_slide_layouts_();
  /// The part at @p path from the package, parsed once and written by `save`;
  /// null where the package has none.
  pugi::xml_node written_part_(const AbsPath &path);
  /// The `r:id` the slide-id list names the slide @p slide_id by.
  [[nodiscard]] std::string
  slide_relationship_id_(ElementIdentifier slide_id) const;
  /// Leaves the part at @p path and its relationships part out of `save`,
  /// with the content type of the part.
  void drop_part_(const AbsPath &path);
};

} // namespace odr::internal::ooxml::presentation
