#include <functional>
#include <internal/odf/odf_element.h>
#include <internal/util/map_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
ElementAdapter *slide_adapter();
ElementAdapter *sheet_adapter();
ElementAdapter *page_adapter();
ElementAdapter *table_adapter();
ElementAdapter *spreadsheet_table_adapter();
ElementAdapter *table_column_adapter();
ElementAdapter *table_row_adapter();
ElementAdapter *table_cell_adapter();

class DefaultAdapter : public ElementAdapter {
public:
  DefaultAdapter(const ElementType element_type,
                 std::unordered_map<std::string, ElementProperty> properties)
      : m_element_type{element_type}, m_properties{std::move(properties)} {}

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
  explicit RootAdapter(ElementAdapter *child_adapter)
      : DefaultAdapter(ElementType::ROOT, {}), m_child_adapter{child_adapter} {}

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
    return {node.previous_sibling("draw:page"), slide_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("draw:page"), slide_adapter()};
  }
};

class SheetAdapter final : public DefaultAdapter {
public:
  SheetAdapter()
      : DefaultAdapter(ElementType::SHEET,
                       {{"table:name", ElementProperty::NAME}}) {}

  [[nodiscard]] Element first_child(const pugi::xml_node node) const final {
    return {node, spreadsheet_table_adapter()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table"), sheet_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table"), sheet_adapter()};
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
    return {node.previous_sibling("draw:page"), page_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("draw:page"), page_adapter()};
  }
};

class TextAdapter final : public DefaultAdapter {
public:
  TextAdapter() : DefaultAdapter(ElementType::TEXT, {}) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    return {{ElementProperty::TEXT, node.text().as_string()}};
  }
};

class SpaceAdapter final : public DefaultAdapter {
public:
  SpaceAdapter() : DefaultAdapter(ElementType::TEXT, {}) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node node) const override {
    // TODO optimize
    const auto count = node.attribute("text:c").as_uint(1);
    return {{ElementProperty::TEXT, std::string(count, ' ')}};
  }
};

class TabAdapter final : public DefaultAdapter {
public:
  TabAdapter() : DefaultAdapter(ElementType::TEXT, {}) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const pugi::xml_node /*node*/) const override {
    return {{ElementProperty::TEXT, "\t"}};
  }
};

class TableAdapter : public DefaultAdapter {
public:
  TableAdapter()
      : DefaultAdapter(ElementType::TABLE,
                       {{"table:style-name", ElementProperty::STYLE_NAME}}) {}

  [[nodiscard]] Element first_child(const pugi::xml_node /*node*/) const final {
    return {};
  }
};

class SpreadsheetTableAdapter final : public TableAdapter {
public:
  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    return {node, sheet_adapter()};
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

class TableColumnAdapter final : public DefaultAdapter {
public:
  TableColumnAdapter()
      : DefaultAdapter(
            ElementType::TABLE_COLUMN,
            {{"table:style-name", ElementProperty::STYLE_NAME},
             {"table:default-cell-style-name",
              ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME}}) {}

  [[nodiscard]] Element parent(const pugi::xml_node node) const final {
    return {node.parent(), table_adapter()};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node /*node*/) const final {
    return {};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-column"),
            table_column_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-column"), table_column_adapter()};
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
    return {node.parent(), table_adapter()};
  }

  [[nodiscard]] Element first_child(const pugi::xml_node node) const final {
    return {node.child("table:table-cell"), table_cell_adapter()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-row"), table_row_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-row"), table_row_adapter()};
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
    return {node.parent(), table_row_adapter()};
  }

  [[nodiscard]] Element
  previous_sibling(const pugi::xml_node node) const final {
    return {node.previous_sibling("table:table-cell"), table_cell_adapter()};
  }

  [[nodiscard]] Element next_sibling(const pugi::xml_node node) const final {
    return {node.next_sibling("table:table-cell"), table_cell_adapter()};
  }
};

ElementAdapter *slide_adapter() {
  static SlideAdapter adapter;
  return &adapter;
}

ElementAdapter *sheet_adapter() {
  static SheetAdapter adapter;
  return &adapter;
}

ElementAdapter *page_adapter() {
  static PageAdapter adapter;
  return &adapter;
}

ElementAdapter *table_adapter() {
  static TableAdapter adapter;
  return &adapter;
}

ElementAdapter *spreadsheet_table_adapter() {
  static SpreadsheetTableAdapter adapter;
  return &adapter;
}

ElementAdapter *table_column_adapter() {
  static TableColumnAdapter adapter;
  return &adapter;
}

ElementAdapter *table_row_adapter() {
  static TableRowAdapter adapter;
  return &adapter;
}

ElementAdapter *table_cell_adapter() {
  static TableCellAdapter adapter;
  return &adapter;
}
} // namespace

ElementAdapter *ElementAdapter::default_adapter(const pugi::xml_node node) {
  static RootAdapter text_root_adapter(nullptr);
  static RootAdapter presentation_root_adapter(slide_adapter());
  static RootAdapter spreadsheet_root_adapter(sheet_adapter());
  static RootAdapter drawing_root_adapter(page_adapter());
  static DefaultAdapter paragraph_adapter(
      ElementType::PARAGRAPH,
      {{"text:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter span_adapter(
      ElementType::SPAN, {{"text:style-name", ElementProperty::STYLE_NAME}});
  static SpaceAdapter space_adapter;
  static TabAdapter tab_adapter;
  static DefaultAdapter line_break_adapter(ElementType::LINE_BREAK, {});
  static DefaultAdapter link_adapter(
      ElementType::LINK, {{"xlink:href", ElementProperty::HREF},
                          {"text:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter bookmark_adapter(
      ElementType::BOOKMARK, {{"text:name", ElementProperty::NAME}});
  static DefaultAdapter list_adapter(ElementType::LIST, {});
  static DefaultAdapter list_item_adapter(ElementType::LIST_ITEM, {});
  static DefaultAdapter frame_adapter(
      ElementType::FRAME, {{"text:anchor-type", ElementProperty::ANCHOR_TYPE},
                           {"svg:x", ElementProperty::X},
                           {"svg:y", ElementProperty::Y},
                           {"svg:width", ElementProperty::WIDTH},
                           {"svg:height", ElementProperty::HEIGHT},
                           {"draw:z-index", ElementProperty::Z_INDEX}});
  static DefaultAdapter image_adapter(ElementType::IMAGE,
                                      {{"xlink:href", ElementProperty::HREF}});
  static DefaultAdapter rect_adapter(
      ElementType::RECT, {{"svg:x", ElementProperty::X},
                          {"svg:y", ElementProperty::Y},
                          {"svg:width", ElementProperty::WIDTH},
                          {"svg:height", ElementProperty::HEIGHT},
                          {"draw:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter line_adapter(
      ElementType::LINE, {{"svg:x1", ElementProperty::X1},
                          {"svg:y1", ElementProperty::Y1},
                          {"svg:x2", ElementProperty::X2},
                          {"svg:y2", ElementProperty::Y2},
                          {"draw:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter circle_adapter(
      ElementType::CIRCLE, {{"svg:x", ElementProperty::X},
                            {"svg:y", ElementProperty::Y},
                            {"svg:width", ElementProperty::WIDTH},
                            {"svg:height", ElementProperty::HEIGHT},
                            {"draw:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter custom_shape_adapter(
      ElementType::CUSTOM_SHAPE,
      {{"svg:x", ElementProperty::X},
       {"svg:y", ElementProperty::Y},
       {"svg:width", ElementProperty::WIDTH},
       {"svg:height", ElementProperty::HEIGHT},
       {"draw:style-name", ElementProperty::STYLE_NAME}});
  static DefaultAdapter group_adapter(ElementType::GROUP, {});
  static TextAdapter text_adapter;

  static std::unordered_map<std::string, ElementAdapter *>
      element_adapter_table{
          {"office:text", &text_root_adapter},
          {"office:presentation", &presentation_root_adapter},
          {"office:spreadsheet", &spreadsheet_root_adapter},
          {"office:drawing", &drawing_root_adapter},
          {"text:p", &paragraph_adapter},
          {"text:h", &paragraph_adapter},
          {"text:span", &span_adapter},
          {"text:s", &space_adapter},
          {"text:tab", &tab_adapter},
          {"text:line-break", &line_break_adapter},
          {"text:a", &link_adapter},
          {"text:bookmark", &bookmark_adapter},
          {"text:bookmark-start", &bookmark_adapter},
          {"text:list", &list_adapter},
          {"text:list-item", &list_item_adapter},
          {"text:index-title", &paragraph_adapter},
          {"text:table-of-content", &group_adapter},
          {"text:index-body", &group_adapter},
          {"table:table", table_adapter()},
          {"table:table-column", table_column_adapter()},
          {"table:table-row", table_row_adapter()},
          {"table:table-cell", table_cell_adapter()},
          {"draw:frame", &frame_adapter},
          {"draw:image", &image_adapter},
          {"draw:rect", &rect_adapter},
          {"draw:line", &line_adapter},
          {"draw:circle", &circle_adapter},
          {"draw:custom-shape", &custom_shape_adapter},
          {"draw:text-box", &group_adapter},
          {"draw:g", &group_adapter},
      };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return &text_adapter;
  }

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

ElementAdapter *TableElement::adapter() { return table_column_adapter(); }

TableElement::TableElement() = default;

TableElement::TableElement(const pugi::xml_node node)
    : ElementBase<TableElement>(node) {}

Element TableElement::element() const { return {m_node, table_adapter()}; }

ElementType TableElement::type() const { return ElementType::TABLE; }

TableColumnElementRange TableElement::columns() const {
  return TableColumnElementRange(
      TableColumnElement(m_node.child("table:table-column")));
}

TableRowElementRange TableElement::rows() const {
  return TableRowElementRange(TableRowElement(m_node.child("table:table-row")));
}

ElementAdapter *TableColumnElement::adapter() { return table_column_adapter(); }

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(const pugi::xml_node node)
    : ElementBase<TableColumnElement>(node) {}

Element TableColumnElement::element() const {
  return {m_node, table_column_adapter()};
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

ElementAdapter *TableRowElement::adapter() { return table_row_adapter(); }

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(const pugi::xml_node node)
    : ElementBase<TableRowElement>(node) {}

Element TableRowElement::element() const {
  return {m_node, table_column_adapter()};
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

ElementAdapter *TableCellElement::adapter() { return table_cell_adapter(); }

TableCellElement::TableCellElement() = default;

TableCellElement::TableCellElement(const pugi::xml_node node)
    : ElementBase<TableCellElement>(node) {}

Element TableCellElement::element() const {
  return {m_node, table_column_adapter()};
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
