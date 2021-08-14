#include <functional>
#include <internal/odf/odf_element.h>
#include <internal/util/map_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
class DefaultAdapter : public ElementAdapter {
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
  [[nodiscard]] virtual ElementAdapter *
  parent_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual ElementAdapter *
  first_child_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual ElementAdapter *
  previous_sibling_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

  [[nodiscard]] virtual ElementAdapter *
  next_sibling_adapter_(const pugi::xml_node node) const {
    return default_adapter(node);
  }

private:
  ElementType m_element_type;
  std::unordered_map<std::string, ElementProperty> m_properties;
};

class TextAdapter : public DefaultAdapter {
public:
  TextAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = node.text().as_string();

    return result;
  }
};

class SpaceAdapter : public DefaultAdapter {
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

class TabAdapter : public DefaultAdapter {
public:
  TabAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = std::string("\t");

    return result;
  }
};
} // namespace

std::shared_ptr<ElementAdapter> ElementAdapter::table_adapter() {
  return DefaultAdapter::create(
      ElementType::TABLE, {{"table:style-name", ElementProperty::STYLE_NAME}});
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_column_adapter() {
  return DefaultAdapter::create(
      ElementType::TABLE_COLUMN,
      {{"table:style-name", ElementProperty::STYLE_NAME},
       {"table:default-cell-style-name",
        ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME}});
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_row_adapter() {
  return DefaultAdapter::create(
      ElementType::TABLE_ROW,
      {{"table:style-name", ElementProperty::STYLE_NAME},
       {"table:number-columns-repeated", ElementProperty::ROWS_REPEATED}});
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_cell_adapter() {
  return DefaultAdapter::create(
      ElementType::TABLE_CELL,
      {{"table:style-name", ElementProperty::STYLE_NAME},
       {"table:number-columns-spanned", ElementProperty::COLUMN_SPAN},
       {"table:number-rows-spanned", ElementProperty::ROW_SPAN},
       {"table:number-columns-repeated", ElementProperty::COLUMNS_REPEATED},
       {"office:value-type", ElementProperty::VALUE_TYPE}});
}

ElementAdapter *ElementAdapter::default_adapter(const pugi::xml_node node) {
  static const auto draw_master_page_attribute = "draw:master-page-name";

  static const auto text_adapter = std::make_shared<TextAdapter>();
  static const auto paragraph_adapter = DefaultAdapter::create(
      ElementType::PARAGRAPH,
      {{"text:style-name", ElementProperty::STYLE_NAME}});
  static const auto bookmark_adapter = DefaultAdapter::create(
      ElementType::BOOKMARK, {{"text:name", ElementProperty::NAME}});

  static std::unordered_map<std::string, std::shared_ptr<ElementAdapter>>
      element_adapter_table{
          {"office:text", nullptr},         // ElementType::ROOT
          {"office:presentation", nullptr}, // ElementType::ROOT
          {"office:spreadsheet", nullptr},  // ElementType::ROOT
          {"office:drawing", nullptr},      // ElementType::ROOT
          {"text:p", paragraph_adapter},
          {"text:h", paragraph_adapter},
          {"text:span",
           DefaultAdapter::create(
               ElementType::SPAN,
               {{"text:style-name", ElementProperty::STYLE_NAME}})},
          {"text:s", std::make_shared<SpaceAdapter>()},
          {"text:tab", std::make_shared<TabAdapter>()},
          {"text:line-break", DefaultAdapter::create(ElementType::LINE_BREAK)},
          {"text:a", DefaultAdapter::create(
                         ElementType::LINK,
                         {{"xlink:href", ElementProperty::HREF},
                          {"text:style-name", ElementProperty::STYLE_NAME}})},
          {"text:bookmark", bookmark_adapter},
          {"text:bookmark-start", bookmark_adapter},
          {"text:list", DefaultAdapter::create(ElementType::LIST)},
          {"text:list-item", DefaultAdapter::create(ElementType::LIST_ITEM)},
          {"text:index-title", paragraph_adapter},
          {"table:table", table_adapter()},
          {"table:table-column", table_column_adapter()},
          {"table:table-row", table_row_adapter()},
          {"table:table-cell", table_cell_adapter()},
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
                {"draw:style-name", ElementProperty::STYLE_NAME}})},
          {"draw:line",
           DefaultAdapter::create(
               ElementType::LINE,
               {{"svg:x1", ElementProperty::X1},
                {"svg:y1", ElementProperty::Y1},
                {"svg:x2", ElementProperty::X2},
                {"svg:y2", ElementProperty::Y2},
                {"draw:style-name", ElementProperty::STYLE_NAME}})},
          {"draw:circle",
           DefaultAdapter::create(
               ElementType::CIRCLE,
               {{"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {"draw:style-name", ElementProperty::STYLE_NAME}})},
          {"draw:custom-shape",
           DefaultAdapter::create(
               ElementType::CUSTOM_SHAPE,
               {{"svg:x", ElementProperty::X},
                {"svg:y", ElementProperty::Y},
                {"svg:width", ElementProperty::WIDTH},
                {"svg:height", ElementProperty::HEIGHT},
                {"draw:style-name", ElementProperty::STYLE_NAME}})},
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
    : m_node{node}, m_adapter(ElementAdapter::default_adapter(node)) {}

Element::Element(pugi::xml_node node, ElementAdapter *adapter)
    : m_node{std::move(node)}, m_adapter{adapter} {}

bool Element::operator==(const Element &rhs) const {
  return (m_node == rhs.m_node) && (m_adapter == rhs.m_adapter);
}

bool Element::operator!=(const Element &rhs) const {
  return (m_node != rhs.m_node) || (m_adapter != rhs.m_adapter);
}

Element::operator bool() const { return m_node && (m_adapter != nullptr); }

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

ElementIterator Element::begin() const { return ElementIterator(*this); }

ElementIterator Element::end() const { return ElementIterator({}); }

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

std::shared_ptr<ElementAdapter> TableElement::adapter_() {
  return ElementAdapter::table_column_adapter();
}

TableElement::TableElement() = default;

TableElement::TableElement(pugi::xml_node node)
    : ElementBase<TableElement>(std::move(node)) {}

Element TableElement::element() const {
  return Element(m_node, ElementAdapter::table_adapter().get());
}

ElementType TableElement::type() const { return ElementType::TABLE; }

Element TableElement::parent() const { return Element(m_node.parent()); }

Element TableElement::previous_sibling() const {
  // TODO iterate / use adapter
  return Element(m_node.previous_sibling());
}

Element TableElement::next_sibling() const {
  // TODO iterate / use adapter
  return Element(m_node.next_sibling());
}

void TableElement::columns() {
  // TODO
}

void TableElement::rows() {
  // TODO
}

std::shared_ptr<ElementAdapter> TableColumnElement::adapter_() {
  return ElementAdapter::table_column_adapter();
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(pugi::xml_node node)
    : ElementBase<TableColumnElement>(std::move(node)) {}

Element TableColumnElement::element() const {
  return Element(m_node, ElementAdapter::table_column_adapter().get());
}

ElementType TableColumnElement::type() const {
  return ElementType::TABLE_COLUMN;
}

TableElement TableColumnElement::parent() const {
  return TableElement(m_node.parent());
}

TableColumnElement TableColumnElement::previous_sibling() const {
  // TODO iterate / use adapter
  return TableColumnElement(m_node.previous_sibling());
}

TableColumnElement TableColumnElement::next_sibling() const {
  // TODO iterate / use adapter
  return TableColumnElement(m_node.next_sibling());
}

std::shared_ptr<ElementAdapter> TableRowElement::adapter_() {
  return ElementAdapter::table_row_adapter();
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(pugi::xml_node node)
    : ElementBase<TableRowElement>(std::move(node)) {}

Element TableRowElement::element() const {
  return Element(m_node, ElementAdapter::table_column_adapter().get());
}

ElementType TableRowElement::type() const { return ElementType::TABLE_ROW; }

TableElement TableRowElement::parent() const {
  return TableElement(m_node.parent());
}

TableRowElement TableRowElement::previous_sibling() const {
  // TODO iterate / use adapter
  return TableRowElement(m_node.previous_sibling());
}

TableRowElement TableRowElement::next_sibling() const {
  // TODO iterate / use adapter
  return TableRowElement(m_node.next_sibling());
}

std::shared_ptr<ElementAdapter> TableCellElement::adapter_() {
  return ElementAdapter::table_cell_adapter();
}

TableCellElement::TableCellElement() = default;

TableCellElement::TableCellElement(pugi::xml_node node)
    : ElementBase<TableCellElement>(std::move(node)) {}

Element TableCellElement::element() const {
  return Element(m_node, ElementAdapter::table_column_adapter().get());
}

ElementType TableCellElement::type() const { return ElementType::TABLE_CELL; }

TableRowElement TableCellElement::parent() const {
  return TableRowElement(m_node.parent());
}

TableCellElement TableCellElement::previous_sibling() const {
  // TODO iterate / use adapter
  return TableCellElement(m_node.previous_sibling());
}

TableCellElement TableCellElement::next_sibling() const {
  // TODO iterate / use adapter
  return TableCellElement(m_node.next_sibling());
}

ElementIterator::ElementIterator(Element element)
    : m_element{std::move(element)} {}

ElementIterator &ElementIterator::operator++() {
  m_element = m_element.next_sibling();
  return *this;
}

ElementIterator ElementIterator::operator++(int) & {
  ElementIterator result = *this;
  operator++();
  return result;
}

Element &ElementIterator::operator*() { return m_element; }

Element *ElementIterator::operator->() { return &m_element; }

bool ElementIterator::operator==(const ElementIterator &rhs) const {
  return m_element == rhs.m_element;
}

bool ElementIterator::operator!=(const ElementIterator &rhs) const {
  return m_element != rhs.m_element;
}

} // namespace odr::internal::odf
