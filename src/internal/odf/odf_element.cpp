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
      if (auto adapter = default_adapter(node); adapter != nullptr) {
        return {node, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const override {
    for (pugi::xml_node first_child = node.first_child(); first_child;
         first_child = first_child.next_sibling()) {
      if (auto adapter = default_adapter(node); adapter != nullptr) {
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
      if (auto adapter = default_adapter(node); adapter != nullptr) {
        return {node, adapter};
      }
    }
    return {};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const override {
    for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
         next_sibling = next_sibling.next_sibling()) {
      if (auto adapter = default_adapter(node); adapter != nullptr) {
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
      const pugi::xml_node /*node*/,
      std::unordered_map<ElementProperty, std::any> /*properties*/)
      const override {
    // TODO
  }

private:
  ElementType m_element_type;
  std::unordered_map<std::string, ElementProperty> m_properties;
};

class RootAdapter final : public DefaultAdapter {
public:
  RootAdapter() : DefaultAdapter(ElementType::ROOT) {}
  explicit RootAdapter(ElementAdapter *child_adapter)
      : DefaultAdapter(ElementType::ROOT), m_child_adapter{child_adapter} {}

  [[nodiscard]] Element parent(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const final {
    if (m_child_adapter == nullptr) {
      return DefaultAdapter::first_child(node);
    }
    return {node.first_child(), m_child_adapter};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element
  next_sibling(const pugi::xml_node /*node*/) const final {
    return {};
  }

private:
  ElementAdapter *m_child_adapter{nullptr};
};

class SlideAdapter final : public DefaultAdapter {
public:
  SlideAdapter()
      : DefaultAdapter(
            ElementType::SLIDE,
            {{"draw:name", ElementProperty::NAME},
             {"draw:style-name", ElementProperty::STYLE_NAME},
             {"draw:master-page-name", ElementProperty::MASTER_PAGE_NAME}}) {}

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("draw:page"), slide_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("draw:page"), slide_adapter().get()};
  }
};

class SheetAdapter final : public DefaultAdapter {
public:
  SheetAdapter()
      : DefaultAdapter(ElementType::SHEET,
                       {{"table:name", ElementProperty::NAME}}) {}

  [[nodiscard]] Element first_child(const pugi::xml_node node) const final {
    return {node, table_adapter().get()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table"), slide_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table"), slide_adapter().get()};
  }
};

class PageAdapter final : public DefaultAdapter {
public:
  PageAdapter()
      : DefaultAdapter(
            ElementType::PAGE,
            {{"draw:name", ElementProperty::NAME},
             {"draw:style-name", ElementProperty::STYLE_NAME},
             {"draw:master-page-name", ElementProperty::MASTER_PAGE_NAME}}) {}

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("draw:page"), slide_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("draw:page"), slide_adapter().get()};
  }
};

class TextAdapter final : public DefaultAdapter {
public:
  TextAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = node.text().as_string();

    return result;
  }
};

class SpaceAdapter final : public DefaultAdapter {
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

class TabAdapter final : public DefaultAdapter {
public:
  TabAdapter() : DefaultAdapter(ElementType::TEXT) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    auto result = DefaultAdapter::properties(node);

    result[ElementProperty::TEXT] = std::string("\t");

    return result;
  }
};

class TableAdapter final : public DefaultAdapter {
public:
  TableAdapter()
      : DefaultAdapter(ElementType::TABLE,
                       {{"table:style-name", ElementProperty::STYLE_NAME}}) {}

  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    // TODO check if sheet is parent
    return DefaultAdapter::parent(node);
  }

  [[nodiscard]] Element first_child(const pugi::xml_node /*node*/) const final {
    return {};
  }
};

class TableColumnAdapter final : public DefaultAdapter {
public:
  TableColumnAdapter()
      : DefaultAdapter(
            ElementType::TABLE_COLUMN,
            {{"table:style-name", ElementProperty::STYLE_NAME},
             {"table:default-cell-style-name",
              ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME}}) {}

  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    return {node.parent(), table_adapter().get()};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-column"),
            table_column_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-column"),
            table_column_adapter().get()};
  }
};

class TableRowAdapter final : public DefaultAdapter {
public:
  TableRowAdapter()
      : DefaultAdapter(ElementType::TABLE_ROW,
                       {{"table:style-name", ElementProperty::STYLE_NAME},
                        {"table:number-columns-repeated",
                         ElementProperty::ROWS_REPEATED}}) {}

  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    return {node.parent(), table_adapter().get()};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const final {
    return {node.child("table:table-cell"), table_cell_adapter().get()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-row"),
            table_row_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-row"), table_row_adapter().get()};
  }
};

class TableCellAdapter final : public DefaultAdapter {
public:
  TableCellAdapter()
      : DefaultAdapter(
            ElementType::TABLE_CELL,
            {{"table:style-name", ElementProperty::STYLE_NAME},
             {"table:number-columns-spanned", ElementProperty::COLUMN_SPAN},
             {"table:number-rows-spanned", ElementProperty::ROW_SPAN},
             {"table:number-columns-repeated",
              ElementProperty::COLUMNS_REPEATED},
             {"office:value-type", ElementProperty::VALUE_TYPE}}) {}

  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    return {node.parent(), table_row_adapter().get()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-cell"),
            table_cell_adapter().get()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-cell"), table_cell_adapter().get()};
  }
};
} // namespace

std::shared_ptr<ElementAdapter> ElementAdapter::slide_adapter() {
  static auto adapter = std::make_shared<SlideAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::sheet_adapter() {
  static auto adapter = std::make_shared<SheetAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::page_adapter() {
  static auto adapter = std::make_shared<PageAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_adapter() {
  static auto adapter = std::make_shared<TableAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_column_adapter() {
  static auto adapter = std::make_shared<TableColumnAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_row_adapter() {
  static auto adapter = std::make_shared<TableRowAdapter>();
  return adapter;
}

std::shared_ptr<ElementAdapter> ElementAdapter::table_cell_adapter() {
  static auto adapter = std::make_shared<TableCellAdapter>();
  return adapter;
}

ElementAdapter *ElementAdapter::default_adapter(const pugi::xml_node node) {
  static const auto text_adapter = std::make_shared<TextAdapter>();
  static const auto paragraph_adapter = DefaultAdapter::create(
      ElementType::PARAGRAPH,
      {{"text:style-name", ElementProperty::STYLE_NAME}});
  static const auto bookmark_adapter = DefaultAdapter::create(
      ElementType::BOOKMARK, {{"text:name", ElementProperty::NAME}});

  static std::unordered_map<std::string, std::shared_ptr<ElementAdapter>>
      element_adapter_table{
          {"office:text", std::make_shared<RootAdapter>()},
          {"office:presentation",
           std::make_shared<RootAdapter>(slide_adapter().get())},
          {"office:spreadsheet",
           std::make_shared<RootAdapter>(sheet_adapter().get())},
          {"office:drawing",
           std::make_shared<RootAdapter>(page_adapter().get())},
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

TableElement Element::table() const { return TableElement(m_node); }

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

ElementRange Element::children() const { return ElementRange(*this); }

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

std::shared_ptr<ElementAdapter> TableElement::adapter() {
  return ElementAdapter::table_column_adapter();
}

TableElement::TableElement() = default;

TableElement::TableElement(pugi::xml_node node)
    : ElementBase<TableElement>(std::move(node)) {}

Element TableElement::element() const {
  return Element(m_node, ElementAdapter::table_adapter().get());
}

ElementType TableElement::type() const { return ElementType::TABLE; }

TableColumnElementRange TableElement::columns() const {
  return TableColumnElementRange(
      TableColumnElement(m_node.child("table:table-column")));
}

TableRowElementRange TableElement::rows() const {
  return TableRowElementRange(TableRowElement(m_node.child("table:table-row")));
}

std::shared_ptr<ElementAdapter> TableColumnElement::adapter() {
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
  return TableElement(Base::parent().xml_node());
}

TableColumnElement TableColumnElement::previous_sibling() const {
  return TableColumnElement(Base::previous_sibling().xml_node());
}

TableColumnElement TableColumnElement::next_sibling() const {
  return TableColumnElement(Base::next_sibling().xml_node());
}

std::shared_ptr<ElementAdapter> TableRowElement::adapter() {
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
  return TableElement(Base::parent().xml_node());
}

TableRowElement TableRowElement::previous_sibling() const {
  return TableRowElement(Base::previous_sibling().xml_node());
}

TableRowElement TableRowElement::next_sibling() const {
  return TableRowElement(Base::next_sibling().xml_node());
}

TableCellElementRange TableRowElement::cells() const {
  return TableCellElementRange(
      TableCellElement(m_node.child("table:table-cell")));
}

std::shared_ptr<ElementAdapter> TableCellElement::adapter() {
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
  return TableRowElement(Base::parent().xml_node());
}

TableCellElement TableCellElement::previous_sibling() const {
  return TableCellElement(Base::previous_sibling().xml_node());
}

TableCellElement TableCellElement::next_sibling() const {
  return TableCellElement(Base::next_sibling().xml_node());
}

} // namespace odr::internal::odf
