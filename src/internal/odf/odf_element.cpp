#include <internal/odf/odf_element.h>
#include <internal/util/map_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

class Element::DefaultAdapter : public Element::Adapter {
public:
  explicit DefaultAdapter(const ElementType element_type)
      : m_element_type{element_type} {}

  [[nodiscard]] ElementType type(const pugi::xml_node /*node*/) const final {
    return m_element_type;
  }

  [[nodiscard]] Element parent(const pugi::xml_node node) const override {
    for (pugi::xml_node parent = node.parent(); parent;
         parent = parent.parent()) {
      if (auto adapter = parent_adapter_(node); adapter != nullptr) {
        return Element(node, adapter);
      }
    }
    return {};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const override {
    for (pugi::xml_node first_child = node.first_child(); first_child;
         first_child = first_child.next_sibling()) {
      if (auto adapter = first_child_adapter_(node); adapter != nullptr) {
        return Element(node, adapter);
      }
    }
    return {};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node previous_sibling = node.previous_sibling();
         previous_sibling;
         previous_sibling = previous_sibling.previous_sibling()) {
      if (auto adapter = previous_sibling_adapter_(node); adapter != nullptr) {
        return Element(node, adapter);
      }
    }
    return {};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
         next_sibling = next_sibling.next_sibling()) {
      if (auto adapter = next_sibling_adapter_(node); adapter != nullptr) {
        return Element(node, adapter);
      }
    }
    return {};
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    // TODO
  }

  void update_properties(
      const pugi::xml_node node,
      std::unordered_map<ElementProperty, std::any> properties) const override {
    // TODO
  }

protected:
  [[nodiscard]] virtual Element::Adapter *
  parent_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual Element::Adapter *
  first_child_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual Element::Adapter *
  previous_sibling_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual Element::Adapter *
  next_sibling_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

private:
  ElementType m_element_type;
};

Element::Adapter *Element::default_adapter(const pugi::xml_node node) {
  static std::unordered_map<std::string, Element::Adapter *>
      element_adapter_table{
          {"text:p", nullptr},              // ElementType::PARAGRAPH
          {"text:h", nullptr},              // ElementType::PARAGRAPH
          {"text:span", nullptr},           // ElementType::SPAN
          {"text:s", nullptr},              // ElementType::TEXT
          {"text:tab", nullptr},            // ElementType::TEXT
          {"text:line-break", nullptr},     // ElementType::LINE_BREAK
          {"text:a", nullptr},              // ElementType::LINK
          {"text:bookmark", nullptr},       // ElementType::BOOKMARK
          {"text:bookmark-start", nullptr}, // ElementType::BOOKMARK
          {"text:list", nullptr},           // ElementType::LIST
          {"text:list-item", nullptr},      // ElementType::LIST_ITEM
          {"text:index-title", nullptr},    // ElementType::PARAGRAPH
          {"table:table", nullptr},         // ElementType::TABLE
          {"draw:frame", nullptr},          // ElementType::FRAME
          {"draw:image", nullptr},          // ElementType::IMAGE
          {"draw:rect", nullptr},           // ElementType::RECT
          {"draw:line", nullptr},           // ElementType::LINE
          {"draw:circle", nullptr},         // ElementType::CIRCLE
          {"draw:custom-shape", nullptr},   // ElementType::CUSTOM_SHAPE
          {"office:text", nullptr},         // ElementType::ROOT
          {"office:presentation", nullptr}, // ElementType::ROOT
          {"office:spreadsheet", nullptr},  // ElementType::ROOT
          {"office:drawing", nullptr},      // ElementType::ROOT
      };

  return util::map::lookup_map_default(element_adapter_table, node.name(),
                                       nullptr);
}

Element::Element() = default;

Element::Element(const pugi::xml_node node)
    : m_node{node}, m_adapter(default_adapter(node)) {}

Element::Element(const pugi::xml_node node, Adapter *adapter)
    : m_node{node}, m_adapter{adapter} {}

Element::operator bool() const { return m_node && (m_adapter != nullptr); }

ElementType Element::type() const {
  if (m_adapter == nullptr) {
    return ElementType::NONE;
  }
  return m_adapter->type(m_node);
}

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
  m_adapter->update_properties(m_node, properties);
}

} // namespace odr::internal::odf
