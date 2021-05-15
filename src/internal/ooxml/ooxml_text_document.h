#ifndef ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
#define ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H

#include <internal/abstract/document.h>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlTextDocument final : public virtual abstract::Document {
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

  [[nodiscard]] ElementIdentifier root_element() const final;
  [[nodiscard]] ElementIdentifier first_entry_element() const final;

  [[nodiscard]] ElementType
  element_type(ElementIdentifier element_id) const final;

  [[nodiscard]] ElementIdentifier
  element_parent(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_first_child(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const final;

  void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const final;

  [[nodiscard]] std::shared_ptr<abstract::Table>
  table(ElementIdentifier element_id) const final;

private:
  class PropertyRegistry;

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

  struct Element {
    pugi::xml_node node;
    ElementType type{ElementType::NONE};

    ElementIdentifier parent;
    ElementIdentifier first_child;
    ElementIdentifier previous_sibling;
    ElementIdentifier next_sibling;
  };

  struct TableElementExtension {
    std::uint32_t row;
    std::uint32_t column;
  };

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  std::vector<Element> m_elements;
  ElementIdentifier m_root;

  std::unordered_map<ElementIdentifier, std::shared_ptr<Table>> m_tables;
  std::unordered_map<ElementIdentifier, TableElementExtension>
      m_table_element_extension;

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
  Element *element_(ElementIdentifier element_id);
  const Element *element_(ElementIdentifier element_id) const;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
