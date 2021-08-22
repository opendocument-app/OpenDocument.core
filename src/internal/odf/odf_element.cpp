#include <functional>
#include <internal/odf/odf_element.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
odr::Element create_default_parent_element(pugi::xml_node node,
                                           OpenDocument *document) {
  for (pugi::xml_node parent = node.parent(); parent;
       parent = parent.parent()) {
    if (auto result = create_default_element(parent, document)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_first_child_element(pugi::xml_node node,
                                                OpenDocument *document) {
  for (pugi::xml_node first_child = node.first_child(); first_child;
       first_child = first_child.next_sibling()) {
    if (auto result = create_default_element(first_child, document)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_previous_sibling_element(pugi::xml_node node,
                                                     OpenDocument *document) {
  for (pugi::xml_node previous_sibling = node.previous_sibling();
       previous_sibling;
       previous_sibling = previous_sibling.previous_sibling()) {
    if (auto result = create_default_element(previous_sibling, document)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_next_sibling_element(pugi::xml_node node,
                                                 OpenDocument *document) {
  for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
       next_sibling = next_sibling.next_sibling()) {
    if (auto result = create_default_element(next_sibling, document)) {
      return result;
    }
  }
  return {};
}

std::unordered_map<ElementProperty, std::any> fetch_properties(
    const std::unordered_map<std::string, ElementProperty> &properties,
    pugi::xml_node node) {
  std::unordered_map<ElementProperty, std::any> result;

  for (auto attribute : node.attributes()) {
    auto property_it = properties.find(attribute.name());
    if (property_it == std::end(properties)) {
      continue;
    }
    result[property_it->second] = attribute.value();
  }

  return result;
}

template <typename Derived>
odr::Element construct_default(pugi::xml_node node, OpenDocument *document) {
  return odr::Element(Derived(node, document));
};

template <typename Derived>
odr::Element construct_default_optional(pugi::xml_node node,
                                        OpenDocument *document) {
  if (!node) {
    return {};
  }
  return odr::Element(Derived(node, document));
};

template <typename Derived, ElementType element_type>
class DefaultElement : public abstract::Element {
public:
  DefaultElement(pugi::xml_node node, OpenDocument *document)
      : m_node{node}, m_document{document} {}

  bool operator==(const abstract::Element &rhs) const final {
    return m_node == static_cast<const DefaultElement &>(rhs).m_node;
  }

  bool operator!=(const abstract::Element &rhs) const final {
    return m_node != static_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type() const final { return element_type; }

  [[nodiscard]] odr::Element copy() const override {
    return odr::Element(static_cast<const Derived &>(*this));
  }

  [[nodiscard]] odr::Element parent() const override {
    return create_default_parent_element(m_node, m_document);
  }

  [[nodiscard]] odr::Element first_child() const override {
    return create_default_first_child_element(m_node, m_document);
  }

  [[nodiscard]] odr::Element previous_sibling() const override {
    return create_default_previous_sibling_element(m_node, m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const override {
    return create_default_next_sibling_element(m_node, m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const override {
    return {};
  }

protected:
  pugi::xml_node m_node;
  OpenDocument *m_document;
};

class Slide;
class Sheet;
class Page;
class TableCell;

template <typename Derived>
class RootBase : public DefaultElement<Derived, ElementType::ROOT> {
public:
  RootBase(pugi::xml_node node, OpenDocument *document)
      : DefaultElement<Derived, ElementType::ROOT>{node, document} {}

  [[nodiscard]] odr::Element parent() const final { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const final { return {}; }

  [[nodiscard]] odr::Element next_sibling() const final { return {}; }
};

class TextDocumentRoot final : public RootBase<TextDocumentRoot> {
public:
  TextDocumentRoot(pugi::xml_node node, OpenDocument *document)
      : RootBase{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    return {}; // TODO
  }
};

class PresentationRoot : public RootBase<PresentationRoot> {
public:
  PresentationRoot(pugi::xml_node node, OpenDocument *document)
      : RootBase{node, document} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Slide>(m_node.child("draw:page"),
                                             m_document);
  }
};

class SpreadsheetRoot : public RootBase<SpreadsheetRoot> {
public:
  SpreadsheetRoot(pugi::xml_node node, OpenDocument *document)
      : RootBase{node, document} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Sheet>(m_node.child("table:table"),
                                             m_document);
  }
};

class DrawingRoot : public RootBase<DrawingRoot> {
public:
  DrawingRoot(pugi::xml_node node, OpenDocument *document)
      : RootBase{node, document} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Sheet>(m_node.child("draw:page"),
                                             m_document);
  }
};

class Slide : public DefaultElement<Slide, ElementType::SLIDE> {
public:
  Slide(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<Slide>(
        m_node.previous_sibling("draw:page"), m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<Slide>(m_node.next_sibling("draw:page"),
                                             m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"draw:name", ElementProperty::NAME},
        {"draw:style-name", ElementProperty::STYLE_NAME},
        {"draw:master-page-name", ElementProperty::MASTER_PAGE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Sheet : public DefaultElement<Sheet, ElementType::SHEET> {
public:
  Sheet(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  // TODO first child

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<Sheet>(
        m_node.previous_sibling("table:table"), m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<Sheet>(m_node.next_sibling("table:table"),
                                             m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:name", ElementProperty::NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Page final : public DefaultElement<Page, ElementType::PAGE> {
public:
  Page(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default<Page>(m_node.previous_sibling("draw:page"),
                                   m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default<Page>(m_node.next_sibling("draw:page"),
                                   m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"draw:name", ElementProperty::NAME},
        {"draw:style-name", ElementProperty::STYLE_NAME},
        {"draw:master-page-name", ElementProperty::MASTER_PAGE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Paragraph final
    : public DefaultElement<Paragraph, ElementType::PARAGRAPH> {
public:
  Paragraph(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"text:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Span final : public DefaultElement<Span, ElementType::SPAN> {
public:
  Span(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"text:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Text final : public DefaultElement<Text, ElementType::TEXT> {
public:
  Text(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}
};

class LineBreak final
    : public DefaultElement<LineBreak, ElementType::LINE_BREAK> {
public:
  LineBreak(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}
};

class Link final : public DefaultElement<Link, ElementType::LINK> {
public:
  Link(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"xlink:href", ElementProperty::HREF},
        {"text:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Bookmark final : public DefaultElement<Bookmark, ElementType::BOOKMARK> {
public:
  Bookmark(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"text:name", ElementProperty::NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class List final : public DefaultElement<List, ElementType::LIST> {
public:
  List(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}
};

class ListItem final : public DefaultElement<ListItem, ElementType::LIST_ITEM> {
public:
  ListItem(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}
};

class TableElement final
    : public DefaultElement<TableElement, ElementType::TABLE> {
public:
  TableElement(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class TableColumn final
    : public DefaultElement<TableColumn, ElementType::TABLE_COLUMN> {
public:
  TableColumn(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  odr::Element parent() const final {
    return construct_default<TableElement>(m_node.parent(), m_document);
  }

  odr::Element first_child() const final { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableColumn>(
        m_node.previous_sibling("table:table-column"), m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableColumn>(
        m_node.next_sibling("table:table-column"), m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:default-cell-style-name",
         ElementProperty::TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class TableRow final : public DefaultElement<TableRow, ElementType::TABLE_ROW> {
public:
  TableRow(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  odr::Element parent() const final {
    return construct_default<TableElement>(m_node.parent(), m_document);
  }

  odr::Element first_child() const final {
    return construct_default<TableCell>(m_node.child("table:table-cell"),
                                        m_document);
  }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableCell>(
        m_node.previous_sibling("table:table-row"), m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableCell>(
        m_node.next_sibling("table:table-row"), m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:number-rows-repeated", ElementProperty::ROWS_REPEATED},
    };
    return fetch_properties(properties, m_node);
  }
};

class TableCell final
    : public DefaultElement<TableCell, ElementType::TABLE_CELL> {
public:
  TableCell(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  odr::Element parent() const final {
    return construct_default<TableRow>(m_node.parent(), m_document);
  }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableCell>(
        m_node.previous_sibling("table:table-cell"), m_document);
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableCell>(
        m_node.next_sibling("table:table-cell"), m_document);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"table:style-name", ElementProperty::STYLE_NAME},
        {"table:number-columns-spanned", ElementProperty::COLUMN_SPAN},
        {"table:number-rows-spanned", ElementProperty::ROW_SPAN},
        {"table:number-columns-repeated", ElementProperty::COLUMNS_REPEATED},
        {"office:value-type", ElementProperty::VALUE_TYPE},
    };
    return fetch_properties(properties, m_node);
  }
};

class Frame final : public DefaultElement<Frame, ElementType::FRAME> {
public:
  Frame(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"text:anchor-type", ElementProperty::ANCHOR_TYPE},
        {"svg:x", ElementProperty::X},
        {"svg:y", ElementProperty::Y},
        {"svg:width", ElementProperty::WIDTH},
        {"svg:height", ElementProperty::HEIGHT},
        {"draw:z-index", ElementProperty::Z_INDEX},
    };
    return fetch_properties(properties, m_node);
  }
};

class Image final : public DefaultElement<Image, ElementType::IMAGE> {
public:
  Image(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"xlink:href", ElementProperty::HREF},
    };
    return fetch_properties(properties, m_node);
  }
};

class Rect final : public DefaultElement<Rect, ElementType::RECT> {
public:
  Rect(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"svg:x", ElementProperty::X},
        {"svg:y", ElementProperty::Y},
        {"svg:width", ElementProperty::WIDTH},
        {"svg:height", ElementProperty::HEIGHT},
        {"draw:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Line final : public DefaultElement<Line, ElementType::LINE> {
public:
  Line(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"svg:x1", ElementProperty::X1},
        {"svg:y1", ElementProperty::Y1},
        {"svg:x2", ElementProperty::X2},
        {"svg:y2", ElementProperty::Y2},
        {"draw:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Circle final : public DefaultElement<Circle, ElementType::CIRCLE> {
public:
  Circle(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"svg:x", ElementProperty::X},
        {"svg:y", ElementProperty::Y},
        {"svg:width", ElementProperty::WIDTH},
        {"svg:height", ElementProperty::HEIGHT},
        {"draw:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class CustomShape final
    : public DefaultElement<CustomShape, ElementType::CUSTOM_SHAPE> {
public:
  CustomShape(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    static std::unordered_map<std::string, ElementProperty> properties{
        {"svg:x", ElementProperty::X},
        {"svg:y", ElementProperty::Y},
        {"svg:width", ElementProperty::WIDTH},
        {"svg:height", ElementProperty::HEIGHT},
        {"draw:style-name", ElementProperty::STYLE_NAME},
    };
    return fetch_properties(properties, m_node);
  }
};

class Group final : public DefaultElement<Group, ElementType::GROUP> {
public:
  Group(pugi::xml_node node, OpenDocument *document)
      : DefaultElement{node, document} {}
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

odr::Element odf::create_default_element(pugi::xml_node node,
                                         OpenDocument *document) {
  using Constructor =
      std::function<odr::Element(pugi::xml_node, OpenDocument *)>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"office:text", construct_default<TextDocumentRoot>},
      {"office:presentation", construct_default<PresentationRoot>},
      {"office:spreadsheet", construct_default<SpreadsheetRoot>},
      {"office:drawing", construct_default<DrawingRoot>},
      {"text:p", construct_default<Paragraph>},
      {"text:h", construct_default<Paragraph>},
      {"text:span", construct_default<Span>},
      {"text:s", construct_default<Text>},
      {"text:tab", construct_default<Text>},
      {"text:line-break", construct_default<LineBreak>},
      {"text:a", construct_default<Link>},
      {"text:bookmark", construct_default<Bookmark>},
      {"text:bookmark-start", construct_default<Bookmark>},
      {"text:list", construct_default<List>},
      {"text:list-item", construct_default<ListItem>},
      {"text:index-title", construct_default<Paragraph>},
      {"text:table-of-content", construct_default<Group>},
      {"text:index-body", construct_default<Group>},
      {"table:table", construct_default<TableElement>},
      {"table:table-column", construct_default<TableColumn>},
      {"table:table-row", construct_default<TableRow>},
      {"table:table-cell", construct_default<TableCell>},
      {"draw:frame", construct_default<Frame>},
      {"draw:image", construct_default<Image>},
      {"draw:rect", construct_default<Rect>},
      {"draw:line", construct_default<Line>},
      {"draw:circle", construct_default<Circle>},
      {"draw:custom-shape", construct_default<CustomShape>},
      {"draw:text-box", construct_default<Group>},
      {"draw:g", construct_default<Group>},
  };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return construct_default<Text>(node, document);
  }

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node, document);
  }

  return {};
}

} // namespace odr::internal
