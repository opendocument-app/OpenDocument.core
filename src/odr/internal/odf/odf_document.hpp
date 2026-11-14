#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <pugixml.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal {
struct BlankSlideLayout;
} // namespace odr::internal

namespace odr::internal::odf {

class Document final : public internal::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files,
           EncryptionState encryption_state,
           std::shared_ptr<zip::util::Archive> archive = nullptr);
  /// A flat document: one tree holding both content and styles, no filesystem.
  Document(FileType file_type, DocumentType document_type,
           pugi::xml_document flat_xml);

  ElementRegistry &element_registry();
  StyleRegistry &style_registry();

  [[nodiscard]] const ElementRegistry &element_registry() const;
  [[nodiscard]] const StyleRegistry &style_registry() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;
  /// The language of the default style.
  [[nodiscard]] std::optional<std::string> locale() const override;
  /// What `table:calculation-settings` states, and the ODF defaults where it
  /// states nothing.
  [[nodiscard]] formula::Settings formula_settings() const override;
  /// The `table:named-range` and `table:named-expression` entries of the
  /// document and of each sheet.
  [[nodiscard]] std::vector<formula::Name> formula_names() const override;

  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;

  /// The element holding `office:styles` and `office:master-styles`: the
  /// root of `styles.xml`, or the flat document.
  [[nodiscard]] pugi::xml_node styles_root() const;

  /// Loads and caches the XML part at @p path for `save`; null if absent or
  /// flat.
  /// @throws std::exception if the part cannot be parsed.
  [[nodiscard]] pugi::xml_node part(const AbsPath &path);

  /// A slide outside the root per layout, as `element_insert_slide` makes it.
  [[nodiscard]] std::vector<ElementIdentifier> slide_layouts() const;
  /// A text box outside the root as `element_insert_text_box` makes it.
  [[nodiscard]] ElementIdentifier text_box_layout() const;
  /// The `style:presentation-page-layout` of the @p index -th layout. A
  /// built-in is written into `office:styles` on first use, for a page of
  /// @p width by @p height centimetres.
  pugi::xml_node slide_layout_node(std::size_t index, double width,
                                   double height);

private:
  struct SlideLayout final {
    ElementIdentifier slide{null_element_id};
    pugi::xml_node node; ///< null for a built-in not written yet
    const BlankSlideLayout *builtin{nullptr};
  };

  void init_(pugi::xml_node content_root, pugi::xml_node styles_root);
  void load_layouts_(pugi::xml_node body);

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;
  pugi::xml_node m_styles_root;
  /// `meta:generator`, empty where unstated.
  std::string m_generator;
  /// The parts besides `content.xml` that `save` writes back.
  std::unordered_map<AbsPath, pugi::xml_document> m_parts;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;

  /// what `slide_layouts` and `text_box_layout` hold, outside the package
  pugi::xml_document m_layouts_xml;
  std::vector<SlideLayout> m_slide_layouts;
  ElementIdentifier m_text_box_layout{null_element_id};
};

} // namespace odr::internal::odf
