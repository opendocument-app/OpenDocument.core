#include <functional>
#include <internal/odf/odf_element.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
odr::Element create_default_parent_element(OpenDocument *document,
                                           pugi::xml_node node) {
  for (pugi::xml_node parent = node.parent(); parent;
       parent = parent.parent()) {
    if (auto result = create_default_element(document, parent)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_first_child_element(OpenDocument *document,
                                                pugi::xml_node node) {
  for (pugi::xml_node first_child = node.first_child(); first_child;
       first_child = first_child.next_sibling()) {
    if (auto result = create_default_element(document, first_child)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_previous_sibling_element(OpenDocument *document,
                                                     pugi::xml_node node) {
  for (pugi::xml_node previous_sibling = node.previous_sibling();
       previous_sibling;
       previous_sibling = previous_sibling.previous_sibling()) {
    if (auto result = create_default_element(document, previous_sibling)) {
      return result;
    }
  }
  return {};
}

odr::Element create_default_next_sibling_element(OpenDocument *document,
                                                 pugi::xml_node node) {
  for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
       next_sibling = next_sibling.next_sibling()) {
    if (auto result = create_default_element(document, next_sibling)) {
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
odr::Element construct_default(OpenDocument *document, pugi::xml_node node) {
  return odr::Element(Derived(document, node));
};

template <typename Derived>
odr::Element construct_default_optional(OpenDocument *document,
                                        pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return odr::Element(Derived(document, node));
};

template <typename Derived, ElementType element_type>
class DefaultElement : public abstract::Element {
public:
  DefaultElement(OpenDocument *document, pugi::xml_node node)
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
    return create_default_parent_element(m_document, m_node);
  }

  [[nodiscard]] odr::Element first_child() const override {
    return create_default_first_child_element(m_document, m_node);
  }

  [[nodiscard]] odr::Element previous_sibling() const override {
    return create_default_previous_sibling_element(m_document, m_node);
  }

  [[nodiscard]] odr::Element next_sibling() const override {
    return create_default_next_sibling_element(m_document, m_node);
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
  RootBase(OpenDocument *document, pugi::xml_node node)
      : DefaultElement<Derived, ElementType::ROOT>{document, node} {}

  [[nodiscard]] odr::Element parent() const final { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const final { return {}; }

  [[nodiscard]] odr::Element next_sibling() const final { return {}; }
};

class TextDocumentRoot final : public RootBase<TextDocumentRoot> {
public:
  TextDocumentRoot(OpenDocument *document, pugi::xml_node node)
      : RootBase{document, node} {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    return {}; // TODO
  }
};

class PresentationRoot : public RootBase<PresentationRoot> {
public:
  PresentationRoot(OpenDocument *document, pugi::xml_node node)
      : RootBase{document, node} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Slide>(m_document,
                                             m_node.child("draw:page"));
  }
};

class SpreadsheetRoot : public RootBase<SpreadsheetRoot> {
public:
  SpreadsheetRoot(OpenDocument *document, pugi::xml_node node)
      : RootBase{document, node} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Sheet>(m_document,
                                             m_node.child("table:table"));
  }
};

class DrawingRoot : public RootBase<DrawingRoot> {
public:
  DrawingRoot(OpenDocument *document, pugi::xml_node node)
      : RootBase{document, node} {}

  [[nodiscard]] odr::Element first_child() const final {
    return construct_default_optional<Page>(m_document,
                                            m_node.child("draw:page"));
  }
};

class Slide : public DefaultElement<Slide, ElementType::SLIDE> {
public:
  Slide(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<Slide>(
        m_document, m_node.previous_sibling("draw:page"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<Slide>(m_document,
                                             m_node.next_sibling("draw:page"));
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
  Sheet(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  // TODO first child

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<Sheet>(
        m_document, m_node.previous_sibling("table:table"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<Sheet>(
        m_document, m_node.next_sibling("table:table"));
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
  Page(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default<Page>(m_document,
                                   m_node.previous_sibling("draw:page"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default<Page>(m_document,
                                   m_node.next_sibling("draw:page"));
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
  Paragraph(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Span(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Text(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}
};

class LineBreak final
    : public DefaultElement<LineBreak, ElementType::LINE_BREAK> {
public:
  LineBreak(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}
};

class Link final : public DefaultElement<Link, ElementType::LINK> {
public:
  Link(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Bookmark(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  List(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}
};

class ListItem final : public DefaultElement<ListItem, ElementType::LIST_ITEM> {
public:
  ListItem(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}
};

class TableElement final
    : public DefaultElement<TableElement, ElementType::TABLE> {
public:
  TableElement(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  TableColumn(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  odr::Element parent() const final {
    return construct_default<TableElement>(m_document, m_node.parent());
  }

  odr::Element first_child() const final { return {}; }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableColumn>(
        m_document, m_node.previous_sibling("table:table-column"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableColumn>(
        m_document, m_node.next_sibling("table:table-column"));
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
  TableRow(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  odr::Element parent() const final {
    return construct_default<TableElement>(m_document, m_node.parent());
  }

  odr::Element first_child() const final {
    return construct_default<TableCell>(m_document,
                                        m_node.child("table:table-cell"));
  }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableCell>(
        m_document, m_node.previous_sibling("table:table-row"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableCell>(
        m_document, m_node.next_sibling("table:table-row"));
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
  TableCell(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

  odr::Element parent() const final {
    return construct_default<TableRow>(m_document, m_node.parent());
  }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return construct_default_optional<TableCell>(
        m_document, m_node.previous_sibling("table:table-cell"));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return construct_default_optional<TableCell>(
        m_document, m_node.next_sibling("table:table-cell"));
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
  Frame(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Image(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Rect(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Line(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Circle(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  CustomShape(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}

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
  Group(OpenDocument *document, pugi::xml_node node)
      : DefaultElement{document, node} {}
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

odr::Element odf::create_default_element(OpenDocument *document,
                                         pugi::xml_node node) {
  using Constructor =
      std::function<odr::Element(OpenDocument *, pugi::xml_node)>;

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
    return construct_default<Text>(document, node);
  }

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node);
  }

  return {};
}

} // namespace odr::internal
