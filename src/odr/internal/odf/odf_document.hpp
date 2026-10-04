#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <pugixml.hpp>

#include <memory>
#include <unordered_map>

namespace odr::internal::odf {

class Document final : public internal::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files,
           EncryptionState encryption_state);
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

  /// Loads and caches the XML part at @p path for `save`; null if absent or
  /// flat.
  /// @throws std::exception if the part cannot be parsed.
  [[nodiscard]] pugi::xml_node part(const AbsPath &path);

private:
  void init_(pugi::xml_node content_root, pugi::xml_node styles_root);

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;
  /// The parts besides `content.xml` that `save` writes back.
  std::unordered_map<AbsPath, pugi::xml_document> m_parts;

  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::odf
