#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/ooxml/text/ooxml_text_cursor.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::ooxml::text {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : m_document{document} {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = construct_default_element(document, root, allocator);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

const Style *DocumentCursor::style() const {
  if (!m_document) {
    return nullptr;
  }
  return &m_document->m_style;
}

DocumentCursor::Element *DocumentCursor::construct_default_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  using Constructor =
      std::function<Element *(const Document *document, pugi::xml_node node,
                              const Allocator &allocator)>;

  using Paragraph = DefaultElement<ElementType::PARAGRAPH>;
  using Span = DefaultElement<ElementType::SPAN>;
  using Link = DefaultElement<ElementType::LINK>;
  using Bookmark = DefaultElement<ElementType::BOOKMARK>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"w:body", construct_default<Root>},
      {"w:t", construct_default<Text>},
      {"w:tab", construct_default<Text>},
      {"w:p", construct_default<Paragraph>},
      {"w:r", construct_default<Span>},
      {"w:bookmarkStart", construct_default<Bookmark>},
      {"w:hyperlink", construct_default<Link>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node, allocator);
  }

  return {};
}

DocumentCursor::Element *DocumentCursor::construct_default_parent_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *DocumentCursor::construct_default_first_child_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
DocumentCursor::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *DocumentCursor::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

std::unordered_map<ElementProperty, std::any> DocumentCursor::fetch_properties(
    const std::unordered_map<std::string, ElementProperty> &property_table,
    pugi::xml_node node, const Style *style, const ElementType element_type,
    const char *default_style_name) {
  std::unordered_map<ElementProperty, std::any> result;

  const char *style_name = default_style_name;

  for (auto attribute : node.attributes()) {
    auto property_it = property_table.find(attribute.name());
    if (property_it == std::end(property_table)) {
      continue;
    }
    auto property = property_it->second;
    result[property] = attribute.value();

    if (property == ElementProperty::STYLE_NAME) {
      style_name = attribute.value();
    }
  }

  if (style && style_name) {
    auto style_properties = style->resolve_style(element_type, node);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

struct DocumentCursor::DefaultTraits {
  static std::unordered_map<std::string, ElementProperty> properties_table() {
    static std::unordered_map<std::string, ElementProperty> result{
        // TODO
    };
    return result;
  }
};

template <ElementType _element_type, typename Traits>
class DocumentCursor::DefaultElement : public Element {
public:
  DefaultElement(const Document *, pugi::xml_node node) : m_node{node} {}

  bool operator==(const Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  bool operator!=(const Element &rhs) const override {
    return m_node != *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type() const final { return _element_type; }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const override {
    return fetch_properties(Traits::properties_table(), m_node,
                            document_style(cursor), _element_type);
  }

  Element *first_child(const common::DocumentCursor &cursor,
                       const Allocator &allocator) override {
    return construct_default_first_child_element(document(cursor), m_node,
                                                 allocator);
  }

  Element *previous_sibling(const common::DocumentCursor &cursor,
                            const Allocator &allocator) override {
    return construct_default_previous_sibling_element(document(cursor), m_node,
                                                      allocator);
  }

  Element *next_sibling(const common::DocumentCursor &cursor,
                        const Allocator &allocator) override {
    return construct_default_next_sibling_element(document(cursor), m_node,
                                                  allocator);
  }

  [[nodiscard]] std::string
  text(const common::DocumentCursor &) const override {
    return "";
  }

  Element *master_page(const common::DocumentCursor &,
                       const Allocator &) override {
    return nullptr;
  }

  Element *first_table_column(const common::DocumentCursor &,
                              const Allocator &) override {
    return nullptr;
  }

  Element *first_table_row(const common::DocumentCursor &,
                           const Allocator &) override {
    return nullptr;
  }

  [[nodiscard]] bool
  image_internal(const common::DocumentCursor &) const override {
    return false;
  }

  [[nodiscard]] std::optional<odr::File>
  image_file(const common::DocumentCursor &) const override {
    return {};
  }

  static const Document *document(const common::DocumentCursor &cursor) {
    return dynamic_cast<const DocumentCursor &>(cursor).m_document;
  }

  static const Style *document_style(const common::DocumentCursor &cursor) {
    return dynamic_cast<const DocumentCursor &>(cursor).style();
  }

protected:
  pugi::xml_node m_node;
};

class DocumentCursor::Root : public DefaultElement<ElementType::ROOT> {
public:
  Root(const Document *document, pugi::xml_node node)
      : DefaultElement<ElementType::ROOT>(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const final {
    if (auto style = document_style(cursor)) {
      // TODO page style
    }
    return {};
  }

  Element *previous_sibling(const common::DocumentCursor &,
                            const Allocator &) final {
    return nullptr;
  }

  Element *next_sibling(const common::DocumentCursor &,
                        const Allocator &) final {
    return nullptr;
  }
};

class DocumentCursor::Text final : public DefaultElement<ElementType::TEXT> {
public:
  Text(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  Element *previous_sibling(const common::DocumentCursor &cursor,
                            const Allocator &allocator) final {
    return construct_default_previous_sibling_element(document(cursor),
                                                      first_(), allocator);
  }

  Element *next_sibling(const common::DocumentCursor &cursor,
                        const Allocator &allocator) final {
    return construct_default_next_sibling_element(document(cursor), last_(),
                                                  allocator);
  }

  [[nodiscard]] std::string text(const common::DocumentCursor &) const final {
    std::string result;
    for (auto node = first_(); is_text_(node); node = node.next_sibling()) {
      result += text_(node);
    }
    return result;
  }

private:
  static bool is_text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "w:t") {
      return true;
    }
    if (name == "w:tab") {
      return true;
    }

    return false;
  }

  static std::string text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "w:t") {
      return node.value();
    }
    if (name == "w:tab") {
      return "\t";
    }

    return "";
  }

  [[nodiscard]] pugi::xml_node first_() const {
    auto node = m_node;
    for (; is_text_(node.previous_sibling()); node = node.previous_sibling()) {
    }
    return node;
  }

  [[nodiscard]] pugi::xml_node last_() const {
    auto node = m_node;
    for (; is_text_(node.next_sibling()); node = node.next_sibling()) {
    }
    return node;
  }
};

} // namespace odr::internal::ooxml::text
