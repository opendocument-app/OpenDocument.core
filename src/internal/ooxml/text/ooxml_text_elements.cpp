#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_elements.h>
#include <internal/util/map_util.h>

namespace odr::internal::ooxml::text {

namespace {
class DefaultAdapter : public ElementAdapter {
public:
  explicit DefaultAdapter(const ElementType element_type)
      : m_element_type{element_type} {}

  [[nodiscard]] ElementType type(const pugi::xml_node /*node*/) const final {
    return m_element_type;
  }

  [[nodiscard]] Element parent(const pugi::xml_node node) const override {
    for (pugi::xml_node parent = node.parent(); parent;
         parent = parent.parent()) {
      if (auto adapter = default_adapter(parent); adapter != nullptr) {
        return {parent, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const override {
    for (pugi::xml_node first_child = node.first_child(); first_child;
         first_child = first_child.next_sibling()) {
      if (auto adapter = default_adapter(first_child); adapter != nullptr) {
        return {first_child, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node previous_sibling = node.previous_sibling();
         previous_sibling;
         previous_sibling = previous_sibling.previous_sibling()) {
      if (auto adapter = default_adapter(previous_sibling);
          adapter != nullptr) {
        return {previous_sibling, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
         next_sibling = next_sibling.next_sibling()) {
      if (auto adapter = default_adapter(next_sibling); adapter != nullptr) {
        return {next_sibling, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const override {
    return {};
  }

  void update_properties(
      const pugi::xml_node /*node*/,
      std::unordered_map<ElementProperty, std::any> /*properties*/)
      const override {
    // TODO
  }

private:
  ElementType m_element_type;
};

class RootAdapter final : public DefaultAdapter {
public:
  RootAdapter() : DefaultAdapter(ElementType::ROOT) {}

  [[nodiscard]] Element parent(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element
  next_sibling(const pugi::xml_node /*node*/) const final {
    return {};
  }
};

class TextAdapter final : public DefaultAdapter {
public:
  TextAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const final {
    return {{ElementProperty::TEXT, node.text().as_string()}};
  }
};

class TabAdapter final : public DefaultAdapter {
public:
  TabAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const final {
    return {{ElementProperty::TEXT, "\t"}};
  }
};

class ParagraphAdapter final : public DefaultAdapter {
public:
  ParagraphAdapter() : DefaultAdapter(ElementType::PARAGRAPH) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const final {
    std::unordered_map<ElementProperty, std::any> result;

    // TODO get style name

    return result;
  }
};

class SpanAdapter final : public DefaultAdapter {
public:
  SpanAdapter() : DefaultAdapter(ElementType::SPAN) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const final {
    std::unordered_map<ElementProperty, std::any> result;

    // TODO get style name

    return result;
  }
};

class BookmarkAdapter final : public DefaultAdapter {
public:
  BookmarkAdapter() : DefaultAdapter(ElementType::BOOKMARK) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const final {
    std::unordered_map<ElementProperty, std::any> result;

    // TODO

    return result;
  }
};

class LinkAdapter final : public DefaultAdapter {
public:
  LinkAdapter() : DefaultAdapter(ElementType::LINK) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const final {
    std::unordered_map<ElementProperty, std::any> result;

    // TODO

    return result;
  }
};
} // namespace

ElementAdapter *ElementAdapter::default_adapter(const pugi::xml_node node) {
  static RootAdapter root_adapter;
  static TextAdapter text_adapter;
  static TabAdapter tab_adapter;
  static ParagraphAdapter paragraph_adapter;
  static SpanAdapter span_adapter;
  static BookmarkAdapter bookmark_adapter;
  static LinkAdapter link_adapter;

  static std::unordered_map<std::string, ElementAdapter *>
      element_adapter_table{
          {"w:body", &root_adapter},
          {"w:t", &text_adapter},
          {"w:tab", &tab_adapter},
          {"w:p", &paragraph_adapter},
          {"w:r", &span_adapter},
          {"w:bookmarkStart", &bookmark_adapter},
          {"w:hyperlink", &link_adapter},
      };

  return util::map::lookup_map_default(element_adapter_table, node.name(),
                                       nullptr);
}

Element::Element() = default;

Element::Element(const pugi::xml_node node)
    : m_node{node}, m_adapter(ElementAdapter::default_adapter(node)) {}

Element::Element(const pugi::xml_node node, ElementAdapter *adapter)
    : m_node{node}, m_adapter{adapter} {}

bool Element::operator==(const Element &rhs) const {
  // TODO check adapter?
  return m_node == rhs.m_node;
}

bool Element::operator!=(const Element &rhs) const {
  // TODO check adapter?
  return m_node != rhs.m_node;
}

Element::operator bool() const {
  // TODO check adapter?
  return m_node;
}

ElementType Element::type() const {
  if (m_adapter == nullptr) {
    return ElementType::NONE;
  }
  return m_adapter->type(m_node);
}

pugi::xml_node Element::xml_node() const { return m_node; }

ElementAdapter *Element::adapter() const { return m_adapter; }

Element Element::parent() const {
  if (m_adapter == nullptr) {
    return {};
  }
  return m_adapter->parent(m_node);
}

Element Element::first_child() const {
  if (m_adapter == nullptr) {
    return {};
  }
  return m_adapter->first_child(m_node);
}

Element Element::previous_sibling() const {
  if (m_adapter == nullptr) {
    return {};
  }
  return m_adapter->previous_sibling(m_node);
}

Element Element::next_sibling() const {
  if (m_adapter == nullptr) {
    return {};
  }
  return m_adapter->next_sibling(m_node);
}

ElementRange Element::children() const { return ElementRange(first_child()); }

std::unordered_map<ElementProperty, std::any> Element::properties() const {
  if (m_adapter == nullptr) {
    return {};
  }
  return m_adapter->properties(m_node);
}

void Element::update_properties(
    std::unordered_map<ElementProperty, std::any> properties) const {
  if (m_adapter == nullptr) {
    return;
  }
  m_adapter->update_properties(m_node, std::move(properties));
}

} // namespace odr::internal::ooxml::text
