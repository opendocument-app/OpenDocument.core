#ifndef ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
#define ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H

#include <internal/common/document.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlTextDocument final : public common::Document {
public:
  explicit OfficeOpenXmlTextDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const final;

  void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const final;

private:
  class Style final {
  public:
    Style();

    explicit Style(pugi::xml_node styles_root);

    [[nodiscard]] std::unordered_map<ElementProperty, std::any>
    resolve_style(ElementType element_type, pugi::xml_node element) const;

  private:
    struct Entry {
      std::shared_ptr<Entry> m_parent;
      pugi::xml_node m_node;

      Entry(std::shared_ptr<Entry> parent, pugi::xml_node node);

      void
      properties(ElementType element,
                 std::unordered_map<ElementProperty, std::any> &result) const;
    };

    std::unordered_map<std::string, pugi::xml_node> m_index;

    std::unordered_map<std::string, std::shared_ptr<Entry>> m_styles;

    void generate_indices_(pugi::xml_node styles_root);
    void generate_styles_();
    std::shared_ptr<Entry> generate_style_(const std::string &name,
                                           pugi::xml_node node);
  };

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  std::unordered_map<ElementIdentifier, pugi::xml_node> m_element_nodes;

  Style m_style;

  ElementIdentifier register_element_(pugi::xml_node node,
                                      ElementIdentifier parent,
                                      ElementIdentifier previous_sibling);
  std::pair<ElementIdentifier, ElementIdentifier>
  register_children_(pugi::xml_node node, ElementIdentifier parent,
                     ElementIdentifier previous_sibling);

  ElementIdentifier new_element_(pugi::xml_node node, ElementType type,
                                 ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
  pugi::xml_node element_node_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
