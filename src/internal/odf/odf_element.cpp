#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_element.h>
#include <internal/odf/odf_style.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::odf {

class Element : public abstract::Element {
protected:
  static const Document *document_(const abstract::Document *document) {
    return dynamic_cast<const Document *>(document);
  }

  static const Style *style_(const abstract::Document *cursor) {
    return &dynamic_cast<const Document *>(cursor)->m_style;
  }
};

namespace {

struct DefaultTraits;
template <ElementType, typename = DefaultTraits> class DefaultElement;
template <typename = DefaultTraits> class DefaultRoot;
class TextDocumentRoot;
class PresentationRoot;
class SpreadsheetRoot;
class DrawingRoot;
class MasterPage;
class Slide;
class Sheet;
class Page;
class Text;
class TableElement;
template <ElementType, typename = DefaultTraits> class DefaultTableElement;
class TableColumn;
class TableRow;
class TableCell;
class ImageElement;

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

struct DefaultTraits {
  static const std::unordered_map<std::string, ElementProperty> &
  properties_table() {
    static std::unordered_map<std::string, ElementProperty> result{
        // PARAGRAPH, SPAN, LINK
        {"text:style-name", ElementProperty::STYLE_NAME},
        // PAGE, SLIDE
        {"draw:master-page-name", ElementProperty::MASTER_PAGE_NAME},
        // SHEET
        {"table:name", ElementProperty::NAME},
        // TABLE, TABLE_COLUMN, TABLE_ROW, TABLE_CELL
        {"table:style-name", ElementProperty::STYLE_NAME},
        // TABLE_CELL
        {"office:value-type", ElementProperty::VALUE_TYPE},
        // LINK, IMAGE
        {"xlink:href", ElementProperty::HREF},
        // BOOKMARK
        {"text:name", ElementProperty::NAME},
        // FRAME
        {"text:anchor-type", ElementProperty::ANCHOR_TYPE},
        {"draw:z-index", ElementProperty::Z_INDEX},
        // RECT, CIRCLE, CUSTOM_SHAPE
        {"svg:x", ElementProperty::X},
        {"svg:y", ElementProperty::Y},
        {"svg:width", ElementProperty::WIDTH},
        {"svg:height", ElementProperty::HEIGHT},
        // LINE
        {"svg:x1", ElementProperty::X1},
        {"svg:y1", ElementProperty::Y1},
        {"svg:x2", ElementProperty::X2},
        {"svg:y2", ElementProperty::Y2},
        // PAGE, SLIDE, RECT, LINE, CIRCLE, CUSTOM_SHAPE
        {"draw:style-name", ElementProperty::STYLE_NAME},
        // master page elements
        {"presentation:placeholder", ElementProperty::PLACEHOLDER},
    };
    return result;
  }
};

template <ElementType _element_type, typename Traits>
class DefaultElement : public Element {
public:
  DefaultElement(const Document *, pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] bool equals(const abstract::Document *,
                            const abstract::Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::Document *document) const override {
    return fetch_properties(Traits::properties_table(), m_node,
                            style_(document), _element_type);
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

  [[nodiscard]] const Image *image(const abstract::Document *) const override {
    return nullptr;
  }

protected:
  pugi::xml_node m_node;
};

template <typename Traits>
class DefaultRoot : public DefaultElement<ElementType::ROOT, Traits> {
public:
  DefaultRoot(const Document *document, pugi::xml_node node)
      : DefaultElement<ElementType::ROOT, Traits>(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    return nullptr;
  }
};

class TextDocumentRoot final : public DefaultRoot<> {
public:
  TextDocumentRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::Document *document) const final {
    if (auto style = style_(document)) {
      return style->resolve_master_page(ElementType::ROOT,
                                        style->first_master_page().value());
    }
    return {};
  }
};

class PresentationRoot final : public DefaultRoot<> {
public:
  PresentationRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<odf::Slide>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class SpreadsheetRoot final : public DefaultRoot<> {
public:
  SpreadsheetRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<Sheet>(
        document_(document), m_node.child("table:table"), allocator);
  }
};

class DrawingRoot final : public DefaultRoot<> {
public:
  DrawingRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<Page>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class MasterPage final : public DefaultElement<ElementType::MASTER_PAGE> {
public:
  MasterPage(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}
};

class Slide final : public DefaultElement<ElementType::SLIDE>,
                    public abstract::Element::Slide {
public:
  Slide(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] const Slide *slide(const abstract::Document *) const final {
    return this;
  }

  abstract::Element *master_page(const abstract::Document *document,
                                 const abstract::Element *,
                                 const Allocator &allocator) const final {
    if (auto master_page_name_attr =
            m_node.attribute("draw:master-page-name")) {
      auto master_page_node =
          style_(document)->master_page_node(master_page_name_attr.value());
      return construct_default_optional<MasterPage>(
          document_(document), master_page_node, allocator);
    }
    return nullptr;
  }
};

class Page final : public DefaultElement<ElementType::PAGE> {
public:
  Page(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

class Text final : public DefaultElement<ElementType::TEXT>,
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
    if (node.type() == pugi::node_pcdata) {
      return true;
    }

    std::string name = node.name();

    if (name == "text:s") {
      return true;
    }
    if (name == "text:tab") {
      return true;
    }

    return false;
  }

  static std::string text_(const pugi::xml_node node) {
    if (node.type() == pugi::node_pcdata) {
      return node.value();
    }

    std::string name = node.name();

    if (name == "text:s") {
      const auto count = node.attribute("text:c").as_uint(1);
      return std::string(count, ' ');
    }
    if (name == "text:tab") {
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

class TableElement : public DefaultElement<ElementType::TABLE>,
                     public abstract::Element::Table {
public:
  TableElement(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  abstract::Element *first_child(const abstract::Document *,
                                 const Allocator &) final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *,
             const abstract::Element *) const final {
    TableDimensions result;
    common::TableCursor cursor;

    for (auto column : m_node.children("table:table-column")) {
      const auto columns_repeated =
          column.attribute("table:number-columns-repeated").as_uint(1);
      cursor.add_column(columns_repeated);
    }

    result.columns = cursor.column();
    cursor = {};

    for (auto row : m_node.children("table:table-row")) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);
    }

    result.rows = cursor.row();

    return result;
  }

  abstract::Element *first_column(const abstract::Document *document,
                                  const abstract::Element *,
                                  const Allocator &allocator) const final {
    return construct_default_optional<TableColumn>(
        document_(document), m_node.child("table:table-column"), allocator);
  }

  abstract::Element *first_row(const abstract::Document *document,
                               const abstract::Element *,
                               const Allocator &allocator) const final {
    return construct_default_optional<TableRow>(
        document_(document), m_node.child("table:table-row"), allocator);
  }
};

template <ElementType _element_type, typename Traits>
class DefaultTableElement : public DefaultElement<_element_type, Traits> {
public:
  DefaultTableElement(const Document *document, pugi::xml_node node)
      : DefaultElement<_element_type, Traits>(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = previous_node_()) {
      DefaultElement<_element_type, Traits>::m_node = previous_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    if (m_repeated_index < number_repeated_() - 1) {
      ++m_repeated_index;
      return this;
    }

    if (auto next_sibling = next_node_()) {
      DefaultElement<_element_type, Traits>::m_node = next_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

private:
  std::uint32_t m_repeated_index{0};

  [[nodiscard]] virtual std::uint32_t number_repeated_() const = 0;
  [[nodiscard]] virtual pugi::xml_node previous_node_() const = 0;
  [[nodiscard]] virtual pugi::xml_node next_node_() const = 0;
};

class TableColumn final
    : public DefaultTableElement<ElementType::TABLE_COLUMN> {
public:
  TableColumn(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node) {}

  [[nodiscard]] const char *default_cell_style_name() const {
    return m_node.attribute("table:default-cell-style-name").value();
  }

private:
  [[nodiscard]] std::uint32_t number_repeated_() const final {
    return m_node.attribute("table:number-columns-repeated").as_uint(1);
  }

  [[nodiscard]] pugi::xml_node previous_node_() const final {
    return m_node.previous_sibling("table:table-column");
  }

  [[nodiscard]] pugi::xml_node next_node_() const final {
    return m_node.next_sibling("table:table-column");
  }
};

class TableRow final : public DefaultTableElement<ElementType::TABLE_ROW> {
public:
  TableRow(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node) {}

private:
  [[nodiscard]] std::uint32_t number_repeated_() const final {
    return m_node.attribute("table:number-rows-repeated").as_uint(1);
  }

  [[nodiscard]] pugi::xml_node previous_node_() const final {
    return m_node.previous_sibling("table:table-row");
  }

  [[nodiscard]] pugi::xml_node next_node_() const final {
    return m_node.next_sibling("table:table-row");
  }
};

class TableCell final : public DefaultTableElement<ElementType::TABLE_CELL>,
                        public abstract::Element::TableCell {
public:
  TableCell(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node),
        m_column(document, node.parent().parent().child("table:table-column")) {
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::Document *document) const final {
    auto default_style_name = m_column.default_cell_style_name();
    return fetch_properties(DefaultTraits::properties_table(), m_node,
                            style_(document), ElementType::TABLE_CELL,
                            default_style_name);
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *,
                                     const abstract::Element *) const final {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

private:
  TableColumn m_column;

  [[nodiscard]] std::uint32_t number_repeated_() const final {
    return m_node.attribute("table:number-columns-repeated").as_uint(1);
  }

  [[nodiscard]] pugi::xml_node previous_node_() const final {
    return m_node.previous_sibling("table:table-cell");
  }

  [[nodiscard]] pugi::xml_node next_node_() const final {
    return m_node.next_sibling("table:table-cell");
  }
};

class ImageElement final : public DefaultElement<ElementType::IMAGE>,
                           public abstract::Element::Image {
public:
  ImageElement(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const char *href() const {
    return m_node.attribute("xlink:href").value();
  }

  [[nodiscard]] bool internal(const abstract::Document *document,
                              const abstract::Element *) const final {
    auto doc = document_(document);
    if (!doc || !doc->files()) {
      return false;
    }
    try {
      return doc->files()->is_file(this->href());
    } catch (...) {
    }
    return false;
  }

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *document,
       const abstract::Element *element) const final {
    auto doc = document_(document);
    if (!doc || !internal(document, element)) {
      return {};
    }
    return File(doc->files()->open(this->href()));
  }
};

class Sheet final : public TableElement {
public:
  Sheet(const Document *document, pugi::xml_node node)
      : TableElement(document, node) {}

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return ElementType::SHEET;
  }

  Element *previous_sibling(const abstract::Document *,
                            const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling("table:table")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  Element *next_sibling(const abstract::Document *, const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling("table:table")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

abstract::Element *odf::construct_default_element(const Document *document,
                                                  pugi::xml_node node,
                                                  const Allocator &allocator) {
  using Constructor = std::function<abstract::Element *(
      const Document *document, pugi::xml_node node,
      const Allocator &allocator)>;

  using Paragraph = DefaultElement<ElementType::PARAGRAPH>;
  using Span = DefaultElement<ElementType::SPAN>;
  using LineBreak = DefaultElement<ElementType::LINE_BREAK>;
  using Link = DefaultElement<ElementType::LINK>;
  using Bookmark = DefaultElement<ElementType::BOOKMARK>;
  using List = DefaultElement<ElementType::LIST>;
  using ListItem = DefaultElement<ElementType::LIST_ITEM>;
  using Group = DefaultElement<ElementType::GROUP>;
  using Frame = DefaultElement<ElementType::FRAME>;
  using Rect = DefaultElement<ElementType::RECT>;
  using Line = DefaultElement<ElementType::LINE>;
  using Circle = DefaultElement<ElementType::CIRCLE>;
  using CustomShape = DefaultElement<ElementType::CUSTOM_SHAPE>;

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
      {"draw:image", construct_default<ImageElement>},
      {"draw:rect", construct_default<Rect>},
      {"draw:line", construct_default<Line>},
      {"draw:circle", construct_default<Circle>},
      {"draw:custom-shape", construct_default<CustomShape>},
      {"draw:text-box", construct_default<Group>},
      {"draw:g", construct_default<Group>},
  };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return construct_default<Text>(document, node, allocator);
  }

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node, allocator);
  }

  return {};
}

abstract::Element *odf::construct_default_parent_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_first_child_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

std::unordered_map<ElementProperty, std::any> odf::fetch_properties(
    const std::unordered_map<std::string, ElementProperty> &property_table,
    pugi::xml_node node, const Style *style, const ElementType element_type,
    const char *default_style_name) {
  std::unordered_map<ElementProperty, std::any> result;

  const char *style_name = default_style_name;
  const char *master_page_name = nullptr;

  for (auto attribute : node.attributes()) {
    auto property_it = property_table.find(attribute.name());
    if (property_it == std::end(property_table)) {
      continue;
    }
    auto property = property_it->second;
    result[property] = attribute.value();

    if (property == ElementProperty::STYLE_NAME) {
      style_name = attribute.value();
    } else if (property == ElementProperty::MASTER_PAGE_NAME) {
      master_page_name = attribute.value();
    }
  }

  if (style && style_name) {
    auto style_properties = style->resolve_style(element_type, style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  // TODO this check does not need to happen all the time
  if (style && master_page_name) {
    auto style_properties =
        style->resolve_master_page(element_type, master_page_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

} // namespace odr::internal
