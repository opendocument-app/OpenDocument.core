#include <functional>
#include <internal/odf/odf_element.h>
#include <internal/util/map_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

class Element::DefaultAdapter : public Element::Adapter {
public:
  static std::shared_ptr<DefaultAdapter>
  create(const ElementType element_type) {
    return std::make_shared<DefaultAdapter>(element_type);
  }

  static std::shared_ptr<DefaultAdapter>
  create(const ElementType element_type,
         std::unordered_map<std::string, ElementProperty> properties) {
    return std::make_shared<DefaultAdapter>(element_type,
                                            std::move(properties));
  }

  explicit DefaultAdapter(const ElementType element_type)
      : m_element_type{element_type} {}

  DefaultAdapter(const ElementType element_type,
                 std::unordered_map<std::string, ElementProperty> properties)
      : m_element_type{element_type}, m_properties{std::move(properties)} {}

  [[nodiscard]] ElementType type(const pugi::xml_node /*node*/) const final {
    return m_element_type;
  }

  [[nodiscard]] Element parent(const pugi::xml_node node) const override {
    for (pugi::xml_node parent = node.parent(); parent;
         parent = parent.parent()) {
      if (auto adapter = parent_adapter_(node); adapter != nullptr) {
        return {node, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const override {
    for (pugi::xml_node first_child = node.first_child(); first_child;
         first_child = first_child.next_sibling()) {
      if (auto adapter = first_child_adapter_(node); adapter != nullptr) {
        return {node, adapter};
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
        return {node, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
         next_sibling = next_sibling.next_sibling()) {
      if (auto adapter = next_sibling_adapter_(node); adapter != nullptr) {
        return {node, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    std::unordered_map<ElementProperty, std::any> result;

    for (auto attribute : node.attributes()) {
      auto property_it = m_properties.find(attribute.name());
      if (property_it == std::end(m_properties)) {
        continue;
      }
      result[property_it->second] = attribute.value();
    }

    return result;
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
  std::unordered_map<std::string, ElementProperty> m_properties;
};

class Element::TextAdapter : public Element::DefaultAdapter {
public:
  TextAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = node.text().as_string();

    return result;
  }
};

class Element::SpaceAdapter : public Element::DefaultAdapter {
public:
  SpaceAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    // TODO optimize
    const auto count = node.attribute("text:c").as_uint(1);
    result[ElementProperty::TEXT] = std::string(count, ' ');

    return result;
  }
};

class Element::TabAdapter : public Element::DefaultAdapter {
public:
  TabAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = std::string("\t");

    return result;
  }
};

Element::Adapter *Element::default_adapter(const pugi::xml_node node) {
  static const auto text_style_attribute = "text:style-name";
  static const auto table_style_attribute = "table:style-name";
  static const auto draw_style_attribute = "draw:style-name";
  static const auto draw_master_page_attribute = "draw:master-page-name";

  static const auto text_adapter = std::make_shared<TextAdapter>();
  static const auto paragraph_adapter = DefaultAdapter::create(
      ElementType::PARAGRAPH,
      {{text_style_attribute, ElementProperty::STYLE_NAME}});
  static const auto bookmark_adapter = DefaultAdapter::create(
      ElementType::BOOKMARK, {{"text:name", ElementProperty::NAME}});

  static std::unordered_map<std::string, std::shared_ptr<Element::Adapter>>
      element_adapter_table{
          {"text:p", paragraph_adapter},
          {"text:h", paragraph_adapter},
          {"text:span",
           DefaultAdapter::create(
               ElementType::SPAN,
               {{text_style_attribute, ElementProperty::STYLE_NAME}})},
          {"text:s", std::make_shared<SpaceAdapter>()},
          {"text:tab", std::make_shared<TabAdapter>()},
          {"text:line-break", DefaultAdapter::create(ElementType::LINE_BREAK)},
          {"text:a",
           DefaultAdapter::create(
               ElementType::LINK,
               {{"xlink:href", ElementProperty::HREF},
                {text_style_attribute, ElementProperty::STYLE_NAME}})},
          {"text:bookmark", bookmark_adapter},
          {"text:bookmark-start", bookmark_adapter},
          {"text:list", DefaultAdapter::create(ElementType::LIST)},
          {"text:list-item", DefaultAdapter::create(ElementType::LIST_ITEM)},
          {"text:index-title", paragraph_adapter},
          {"table:table",
           DefaultAdapter::create(
               ElementType::TABLE,
               {{table_style_attribute, ElementProperty::STYLE_NAME}})},
          {"draw:frame",
           DefaultAdapter::create(
               ElementType::FRAME,
               {{"text:anchor-type", ElementProperty::ANCHOR_TYPE},
                {"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {"draw:z-index", ElementProperty::Z_INDEX}})},
          {"draw:image",
           DefaultAdapter::create(ElementType::IMAGE,
                                  {{"xlink:href", ElementProperty::HREF}})},
          {"draw:rect",
           DefaultAdapter::create(
               ElementType::RECT,
               {{"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {draw_style_attribute, ElementProperty::STYLE_NAME}})},
          {"draw:line",
           DefaultAdapter::create(
               ElementType::LINE,
               {{"svg:x1", ElementProperty::X1},
                {"svg:y1", ElementProperty::Y1},
                {"svg:x2", ElementProperty::X2},
                {"svg:y2", ElementProperty::Y2},
                {draw_style_attribute, ElementProperty::STYLE_NAME}})},
          {"draw:circle",
           DefaultAdapter::create(
               ElementType::CIRCLE,
               {{"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {draw_style_attribute, ElementProperty::STYLE_NAME}})},
          {"draw:custom-shape",
           DefaultAdapter::create(
               ElementType::CUSTOM_SHAPE,
               {{"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {draw_style_attribute, ElementProperty::STYLE_NAME}})},
          {"office:text", nullptr},         // ElementType::ROOT
          {"office:presentation", nullptr}, // ElementType::ROOT
          {"office:spreadsheet", nullptr},  // ElementType::ROOT
          {"office:drawing", nullptr},      // ElementType::ROOT
      };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return text_adapter.get();
  }

  return util::map::lookup_map_default(element_adapter_table, node.name(),
                                       nullptr)
      .get();
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
  m_adapter->update_properties(m_node, std::move(properties));
}

} // namespace odr::internal::odf
