#ifndef ODR_INTERNAL_OOXML_TEXT_CURSOR_H
#define ODR_INTERNAL_OOXML_TEXT_CURSOR_H

#include <internal/common/document_cursor.h>

namespace odr::internal::ooxml::text {
class Document;
class Style;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

private:
  const Document *m_document;

  const Style *style() const;

  template <typename Derived>
  static Element *construct_default(const Document *document,
                                    pugi::xml_node node,
                                    const Allocator &allocator) {
    auto alloc = allocator(sizeof(Derived));
    return new (alloc) Derived(document, node);
  }

  template <typename Derived>
  static Element *construct_default_optional(const Document *document,
                                             pugi::xml_node node,
                                             const Allocator &allocator) {
    if (!node) {
      return nullptr;
    }
    return construct_default<Derived>(document, node, allocator);
  }

  static DocumentCursor::Element *
  construct_default_element(const Document *document, pugi::xml_node node,
                            const Allocator &allocator);
  static DocumentCursor::Element *
  construct_default_parent_element(const Document *document,
                                   pugi::xml_node node,
                                   const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_first_child_element(
      const Document *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_previous_sibling_element(
      const Document *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_next_sibling_element(
      const Document *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);

  static std::unordered_map<ElementProperty, std::any> fetch_properties(
      const std::unordered_map<std::string, ElementProperty> &property_table,
      pugi::xml_node node, const Style *style, ElementType element_type,
      const char *default_style_name = nullptr);

  struct DefaultTraits;
  template <ElementType, typename = DefaultTraits> class DefaultElement;
  class Root;
  class Text;
  class ImageElement;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_CURSOR_H
