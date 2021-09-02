#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_element.h>
#include <internal/ooxml/text/ooxml_text_style.h>

namespace odr::internal::ooxml::text {

Element::Element(pugi::xml_node node) : m_node{node} {}

const Document *Element::document_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document);
}

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &dynamic_cast<const Document *>(document)->m_style_registry;
}

namespace {

template <ElementType> class DefaultElement;
class Root;

template <typename Derived>
abstract::Element *construct_default(const Document *document,
                                     pugi::xml_node node,
                                     const Allocator &allocator) {
  auto alloc = allocator(sizeof(Derived));
  return new (alloc) Derived(document, node);
}

template <typename Derived>
abstract::Element *construct_default_optional(const Document *document,
                                              pugi::xml_node node,
                                              const Allocator &allocator) {
  if (!node) {
    return nullptr;
  }
  return construct_default<Derived>(document, node, allocator);
}

template <ElementType _element_type> class DefaultElement : public Element {
public:
  DefaultElement(const Document *, pugi::xml_node node) : Element(node) {}

  [[nodiscard]] bool equals(const abstract::Document *,
                            const abstract::Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *) const override {
    return {}; // TODO
  }

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto name = style_name(document)) {
      return style_(document)->style(*name);
    }
    return nullptr;
  }

  abstract::Element *parent(const abstract::Document *document,
                            const Allocator &allocator) override {
    return construct_default_parent_element(document_(document), m_node,
                                            allocator);
  }

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) override {
    return construct_default_first_child_element(document_(document), m_node,
                                                 allocator);
  }

  abstract::Element *previous_sibling(const abstract::Document *document,
                                      const Allocator &allocator) override {
    return construct_default_previous_sibling_element(document_(document),
                                                      m_node, allocator);
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const Allocator &allocator) override {
    return construct_default_next_sibling_element(document_(document), m_node,
                                                  allocator);
  }

  [[nodiscard]] const Text *text(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Link *link(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Bookmark *
  bookmark(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Slide *slide(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Table *table(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const TableCell *
  table_cell(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Frame *frame(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Rect *rect(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Line *line(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Circle *
  circle(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const CustomShape *
  custom_shape(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Image *image(const abstract::Document *) const override {
    return nullptr;
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  Root(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    return nullptr;
  }

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *) const override {
    return {}; // TODO
  }

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *) const override {
    return nullptr; // TODO
  }
};

class Text final : public DefaultElement<ElementType::text>,
                   public abstract::Element::Text {
public:
  Text(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *document,
                                      const Allocator &allocator) final {
    return construct_default_previous_sibling_element(document_(document),
                                                      first_(), allocator);
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const Allocator &allocator) final {
    return construct_default_next_sibling_element(document_(document), last_(),
                                                  allocator);
  }

  [[nodiscard]] const abstract::Element::Text *
  text(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::string value(const abstract::Document *,
                                  const abstract::Element *) const final {
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
      return node.text().get();
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

} // namespace

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

abstract::Element *text::construct_default_element(const Document *document,
                                                   pugi::xml_node node,
                                                   const Allocator &allocator) {
  using Constructor = std::function<abstract::Element *(
      const Document *document, pugi::xml_node node,
      const Allocator &allocator)>;

  using Paragraph = DefaultElement<ElementType::paragraph>;
  using Span = DefaultElement<ElementType::span>;
  using Link = DefaultElement<ElementType::link>;
  using Bookmark = DefaultElement<ElementType::bookmark>;

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

abstract::Element *text::construct_default_parent_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_first_child_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

} // namespace odr::internal::ooxml
