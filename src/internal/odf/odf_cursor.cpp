#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_style.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::odf {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : m_document{document} {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = construct_default_element(document, root, allocator);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

const Style *DocumentCursor::style() const {
  if (!m_document) {
    return nullptr;
  }
  return &m_document->m_style;
}

DocumentCursor::Element *DocumentCursor::construct_default_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  using Constructor =
      std::function<Element *(const Document *document, pugi::xml_node node,
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

DocumentCursor::Element *DocumentCursor::construct_default_parent_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *DocumentCursor::construct_default_first_child_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
DocumentCursor::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *DocumentCursor::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node, const Allocator &allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

std::unordered_map<ElementProperty, std::any> DocumentCursor::fetch_properties(
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

struct DocumentCursor::DefaultTraits {
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
class DocumentCursor::DefaultElement : public common::DocumentCursor::Element {
public:
  DefaultElement(const Document *, pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] bool
  equals(const abstract::DocumentCursor *,
         const abstract::DocumentCursor::Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType
  type(const abstract::DocumentCursor *) const override {
    return _element_type;
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::DocumentCursor *cursor) const override {
    return fetch_properties(Traits::properties_table(), m_node,
                            document_style(cursor), _element_type);
  }

  Element *first_child(const common::DocumentCursor *cursor,
                       const Allocator &allocator) override {
    return construct_default_first_child_element(document(cursor), m_node,
                                                 allocator);
  }

  Element *previous_sibling(const common::DocumentCursor *cursor,
                            const Allocator &allocator) override {
    return construct_default_previous_sibling_element(document(cursor), m_node,
                                                      allocator);
  }

  Element *next_sibling(const common::DocumentCursor *cursor,
                        const Allocator &allocator) override {
    return construct_default_next_sibling_element(document(cursor), m_node,
                                                  allocator);
  }

  static const Document *document(const abstract::DocumentCursor *cursor) {
    return dynamic_cast<const DocumentCursor *>(cursor)->m_document;
  }

  static const Style *document_style(const abstract::DocumentCursor *cursor) {
    return dynamic_cast<const DocumentCursor *>(cursor)->style();
  }

protected:
  pugi::xml_node m_node;
};

template <typename Traits>
class DocumentCursor::DefaultRoot
    : public DefaultElement<ElementType::ROOT, Traits> {
public:
  DefaultRoot(const Document *document, pugi::xml_node node)
      : DefaultElement<ElementType::ROOT, Traits>(document, node) {}

  Element *previous_sibling(const common::DocumentCursor *,
                            const Allocator &) final {
    return nullptr;
  }

  Element *next_sibling(const common::DocumentCursor *,
                        const Allocator &) final {
    return nullptr;
  }
};

class DocumentCursor::TextDocumentRoot final : public DefaultRoot<> {
public:
  TextDocumentRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::DocumentCursor *cursor) const final {
    if (auto style = document_style(cursor)) {
      return style->resolve_master_page(ElementType::ROOT,
                                        style->first_master_page().value());
    }
    return {};
  }
};

class DocumentCursor::PresentationRoot final : public DefaultRoot<> {
public:
  PresentationRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor *cursor,
                       const Allocator &allocator) final {
    return construct_default_optional<DocumentCursor::Slide>(
        document(cursor), m_node.child("draw:page"), allocator);
  }
};

class DocumentCursor::SpreadsheetRoot final : public DefaultRoot<> {
public:
  SpreadsheetRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor *cursor,
                       const Allocator &allocator) final {
    return construct_default_optional<Sheet>(
        document(cursor), m_node.child("table:table"), allocator);
  }
};

class DocumentCursor::DrawingRoot final : public DefaultRoot<> {
public:
  DrawingRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor *cursor,
                       const Allocator &allocator) final {
    return construct_default_optional<Page>(
        document(cursor), m_node.child("draw:page"), allocator);
  }
};

class DocumentCursor::MasterPage final
    : public DefaultElement<ElementType::MASTER_PAGE> {
public:
  MasterPage(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}
};

class DocumentCursor::Slide final
    : public DefaultElement<ElementType::SLIDE>,
      public common::DocumentCursor::Element::Slide {
public:
  Slide(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  Element *previous_sibling(const common::DocumentCursor *,
                            const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  Element *next_sibling(const common::DocumentCursor *,
                        const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  Element *master_page(const common::DocumentCursor *cursor,
                       const Allocator &allocator) const final {
    if (auto master_page_name_attr =
            m_node.attribute("draw:master-page-name")) {
      auto master_page_node = document_style(cursor)->master_page_node(
          master_page_name_attr.value());
      return construct_default_optional<MasterPage>(
          document(cursor), master_page_node, allocator);
    }
    return nullptr;
  }
};

class DocumentCursor::Page final : public DefaultElement<ElementType::PAGE> {
public:
  Page(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  Element *previous_sibling(const common::DocumentCursor *,
                            const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  Element *next_sibling(const common::DocumentCursor *,
                        const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

class DocumentCursor::Text final
    : public DefaultElement<ElementType::TEXT>,
      public common::DocumentCursor::Element::Text {
public:
  Text(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  Element *previous_sibling(const common::DocumentCursor *cursor,
                            const Allocator &allocator) final {
    return construct_default_previous_sibling_element(document(cursor),
                                                      first_(), allocator);
  }

  Element *next_sibling(const common::DocumentCursor *cursor,
                        const Allocator &allocator) final {
    return construct_default_next_sibling_element(document(cursor), last_(),
                                                  allocator);
  }

  [[nodiscard]] const common::DocumentCursor::Element::Text *
  text(const abstract::DocumentCursor *) const final {
    return this;
  }

  [[nodiscard]] std::string
  value(const abstract::DocumentCursor *,
        const abstract::DocumentCursor::Element *) const final {
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

class DocumentCursor::TableElement
    : public DefaultElement<ElementType::TABLE>,
      public common::DocumentCursor::Element::Table {
public:
  TableElement(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  Element *first_child(const common::DocumentCursor *,
                       const Allocator &) final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const abstract::DocumentCursor *,
                   const abstract::DocumentCursor::Element *) const final {
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

  Element *first_table_column(const common::DocumentCursor *cursor,
                              const Allocator &allocator) const final {
    return construct_default_optional<TableColumn>(
        document(cursor), m_node.child("table:table-column"), allocator);
  }

  Element *first_table_row(const common::DocumentCursor *cursor,
                           const Allocator &allocator) const final {
    return construct_default_optional<TableRow>(
        document(cursor), m_node.child("table:table-row"), allocator);
  }
};

template <ElementType _element_type, typename Traits>
class DocumentCursor::DefaultTableElement
    : public DefaultElement<_element_type, Traits> {
public:
  DefaultTableElement(const Document *document, pugi::xml_node node)
      : DefaultElement<_element_type, Traits>(document, node) {}

  Element *previous_sibling(const common::DocumentCursor *,
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

  Element *next_sibling(const common::DocumentCursor *,
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

class DocumentCursor::TableColumn final
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

class DocumentCursor::TableRow final
    : public DefaultTableElement<ElementType::TABLE_ROW> {
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

class DocumentCursor::TableCell final
    : public DefaultTableElement<ElementType::TABLE_CELL>,
      public common::DocumentCursor::Element::TableCell {
public:
  TableCell(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node),
        m_column(document, node.parent().parent().child("table:table-column")) {
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const abstract::DocumentCursor *cursor) const final {
    auto default_style_name = m_column.default_cell_style_name();
    return fetch_properties(DefaultTraits::properties_table(), m_node,
                            document_style(cursor), ElementType::TABLE_CELL,
                            default_style_name);
  }

  [[nodiscard]] TableDimensions
  table_cell_span(const abstract::DocumentCursor *,
                  const abstract::DocumentCursor::Element *) const final {
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

class DocumentCursor::ImageElement final
    : public DefaultElement<ElementType::IMAGE>,
      public common::DocumentCursor::Element::Image {
public:
  ImageElement(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const char *href() const {
    return m_node.attribute("xlink:href").value();
  }

  [[nodiscard]] bool
  image_internal(const abstract::DocumentCursor *cursor,
                 const abstract::DocumentCursor::Element *) const final {
    auto doc = document(cursor);
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
  image_file(const abstract::DocumentCursor *cursor,
             const abstract::DocumentCursor::Element *element) const final {
    auto doc = document(cursor);
    if (!doc || !image_internal(cursor, element)) {
      return {};
    }
    return File(doc->files()->open(this->href()));
  }
};

class DocumentCursor::Sheet final : public TableElement {
public:
  Sheet(const Document *document, pugi::xml_node node)
      : TableElement(document, node) {}

  [[nodiscard]] ElementType
  type(const abstract::DocumentCursor *) const override {
    return ElementType::SHEET;
  }

  Element *previous_sibling(const common::DocumentCursor *,
                            const Allocator &) final {
    if (auto previous_sibling = m_node.previous_sibling("table:table")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  Element *next_sibling(const common::DocumentCursor *,
                        const Allocator &) final {
    if (auto next_sibling = m_node.next_sibling("table:table")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

} // namespace odr::internal::odf
