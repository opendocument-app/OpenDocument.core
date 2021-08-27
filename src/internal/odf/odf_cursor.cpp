#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_style.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::odf {

namespace {
template <typename Derived>
DocumentCursor::Element *
construct_default(const OpenDocument *document, pugi::xml_node node,
                  DocumentCursor::Allocator allocator) {
  auto alloc = allocator(sizeof(Derived));
  return new (alloc) Derived(document, node);
};

template <typename Derived>
DocumentCursor::Element *
construct_default_optional(const OpenDocument *document, pugi::xml_node node,
                           DocumentCursor::Allocator allocator) {
  if (!node) {
    return nullptr;
  }
  return construct_default<Derived>(document, node, allocator);
};

DocumentCursor::Element *
construct_default_element(const OpenDocument *document, pugi::xml_node node,
                          DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_parent_element(const OpenDocument *document,
                                 pugi::xml_node node,
                                 DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_first_child_element(const OpenDocument *document,
                                      pugi::xml_node node,
                                      DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_previous_sibling_element(const OpenDocument *document,
                                           pugi::xml_node node,
                                           DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_next_sibling_element(const OpenDocument *document,
                                       pugi::xml_node node,
                                       DocumentCursor::Allocator allocator);

std::unordered_map<ElementProperty, std::any> fetch_properties(
    const std::unordered_map<std::string, ElementProperty> &property_table,
    pugi::xml_node node, const Style *style, const ElementType element_type,
    const char *default_style_name = nullptr) {
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
} // namespace

struct DocumentCursor::DefaultTraits {
  static std::unordered_map<std::string, ElementProperty> properties_table() {
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
    };
    return result;
  }
};

template <ElementType _element_type, typename Traits>
class DocumentCursor::DefaultElement : public DocumentCursor::Element {
public:
  pugi::xml_node m_node;

  DefaultElement(const OpenDocument *, pugi::xml_node node) : m_node{node} {}

  bool operator==(const Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  bool operator!=(const Element &rhs) const override {
    return m_node != *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  ElementType type() const final { return _element_type; }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const override {
    return fetch_properties(Traits::properties_table(), m_node,
                            document_style(cursor), _element_type);
  }

  const TableElement *table(const common::DocumentCursor &) const override {
    return nullptr;
  }

  const ImageElement *image(const common::DocumentCursor &) const override {
    return nullptr;
  }

  Element *first_child(const common::DocumentCursor &cursor,
                       DocumentCursor::Allocator allocator) override {
    return construct_default_first_child_element(document(cursor), m_node,
                                                 allocator);
  }

  Element *previous_sibling(const common::DocumentCursor &cursor,
                            DocumentCursor::Allocator allocator) override {
    return construct_default_previous_sibling_element(document(cursor), m_node,
                                                      allocator);
  }

  Element *next_sibling(const common::DocumentCursor &cursor,
                        DocumentCursor::Allocator allocator) override {
    return construct_default_next_sibling_element(document(cursor), m_node,
                                                  allocator);
  }

  static const OpenDocument *document(const common::DocumentCursor &cursor) {
    return static_cast<const DocumentCursor &>(cursor).m_document;
  }

  static const Style *document_style(const common::DocumentCursor &cursor) {
    return static_cast<const DocumentCursor &>(cursor).style();
  }
};

namespace {
template <typename Traits = DocumentCursor::DefaultTraits>
class DefaultRoot
    : public DocumentCursor::DefaultElement<ElementType::ROOT, Traits> {
public:
  DefaultRoot(const OpenDocument *document, pugi::xml_node node)
      : DocumentCursor::DefaultElement<ElementType::ROOT, Traits>(document,
                                                                  node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    return nullptr;
  }
};

class Slide;
class Sheet;
class Page;

class TextDocumentRoot final : public DefaultRoot<> {
public:
  TextDocumentRoot(const OpenDocument *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const final {
    if (auto style = document_style(cursor)) {
      return style->resolve_master_page(ElementType::ROOT,
                                        style->first_master_page().value());
    }
    return {};
  }
};

class PresentationRoot final : public DefaultRoot<> {
public:
  PresentationRoot(const OpenDocument *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor &cursor,
                       DocumentCursor::Allocator allocator) final {
    return construct_default_optional<Slide>(document(cursor),
                                             m_node.first_child(), allocator);
  }
};

class SpreadsheetRoot final : public DefaultRoot<> {
public:
  SpreadsheetRoot(const OpenDocument *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor &cursor,
                       DocumentCursor::Allocator allocator) final {
    return construct_default_optional<Sheet>(document(cursor),
                                             m_node.first_child(), allocator);
  }
};

class DrawingRoot final : public DefaultRoot<> {
public:
  DrawingRoot(const OpenDocument *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  Element *first_child(const common::DocumentCursor &cursor,
                       DocumentCursor::Allocator allocator) final {
    return construct_default_optional<Page>(document(cursor),
                                            m_node.first_child(), allocator);
  }
};

class Slide final : public DocumentCursor::DefaultElement<ElementType::SLIDE> {
public:
  Slide(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

class Sheet final : public DocumentCursor::DefaultElement<ElementType::SHEET> {
public:
  Sheet(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

class Page final : public DocumentCursor::DefaultElement<ElementType::PAGE> {
public:
  Page(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    if (auto next_sibling = m_node.next_sibling()) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }
};

class Text final : public DocumentCursor::DefaultElement<ElementType::TEXT> {
public:
  Text(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &) const final {
    return {
        {ElementProperty::TEXT, text()},
    };
  }

  DocumentCursor::Element *
  previous_sibling(const common::DocumentCursor &cursor,
                   DocumentCursor::Allocator allocator) final {
    return construct_default_previous_sibling_element(document(cursor), left_(),
                                                      allocator);
  }

  DocumentCursor::Element *
  next_sibling(const common::DocumentCursor &cursor,
               DocumentCursor::Allocator allocator) final {
    return construct_default_next_sibling_element(document(cursor), right_(),
                                                  allocator);
  }

  std::string text() const {
    std::string result;
    for (auto node = left_(); is_text_(node); node = node.next_sibling()) {
      result = result + text_(node);
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

  pugi::xml_node left_() const {
    auto node = m_node;
    for (; is_text_(node.previous_sibling()); node = node.previous_sibling()) {
    }
    return node;
  }

  pugi::xml_node right_() const {
    auto node = m_node;
    for (; is_text_(node.next_sibling()); node = node.next_sibling()) {
    }
    return node;
  }
};

class TableElement final
    : public DocumentCursor::DefaultElement<ElementType::TABLE> {
public:
  TableElement(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}
};

class TableColumn final
    : public DocumentCursor::DefaultElement<ElementType::TABLE_COLUMN> {
public:
  TableColumn(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = m_node.previous_sibling("table:table-column")) {
      m_node = previous_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    if (m_repeated_index < number_repeated() - 1) {
      ++m_repeated_index;
      return this;
    }

    if (auto next_sibling = m_node.next_sibling("table:table-column")) {
      m_node = next_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  const char *default_cell_style_name() const {
    return m_node.attribute("table:default-cell-style-name").value();
  }

private:
  std::uint32_t m_repeated_index{0};

  std::uint32_t number_repeated() const {
    return m_node.attribute("table:number-columns-repeated").as_uint(1);
  }
};

class TableRow final
    : public DocumentCursor::DefaultElement<ElementType::TABLE_ROW> {
public:
  TableRow(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  DocumentCursor::Element *previous_sibling(const common::DocumentCursor &,
                                            DocumentCursor::Allocator) final {
    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = m_node.previous_sibling("table:table-row")) {
      m_node = previous_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) final {
    if (m_repeated_index < number_repeated() - 1) {
      ++m_repeated_index;
      return this;
    }

    if (auto next_sibling = m_node.next_sibling("table:table-row")) {
      m_node = next_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

private:
  std::uint32_t m_repeated_index{0};

  std::uint32_t number_repeated() const {
    return m_node.attribute("table:number-rows-repeated").as_uint(1);
  }
};

class TableCell final
    : public DocumentCursor::DefaultElement<ElementType::TABLE_CELL> {
public:
  TableCell(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node), m_column(document, node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const final {
    auto default_style_name = m_column.default_cell_style_name();
    return fetch_properties(DocumentCursor::DefaultTraits::properties_table(),
                            m_node, document_style(cursor),
                            ElementType::TABLE_CELL, default_style_name);
  }

  DocumentCursor::Element *
  previous_sibling(const common::DocumentCursor &cursor,
                   DocumentCursor::Allocator allocator) final {
    m_column.previous_sibling(cursor, allocator);

    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = m_node.previous_sibling("table:table-cell")) {
      m_node = previous_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  DocumentCursor::Element *
  next_sibling(const common::DocumentCursor &cursor,
               DocumentCursor::Allocator allocator) final {
    m_column.next_sibling(cursor, allocator);

    if (m_repeated_index < number_repeated() - 1) {
      ++m_repeated_index;
      return this;
    }

    if (auto next_sibling = m_node.next_sibling("table:table-cell")) {
      m_node = next_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

private:
  std::uint32_t m_repeated_index{0};
  TableColumn m_column;

  std::uint32_t number_columns_spanned() const {
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }

  std::uint32_t number_rows_spanned() const {
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t number_repeated() const {
    return m_node.attribute("table:number-columns-repeated").as_uint(1);
  }
};

class ImageElement final
    : public DocumentCursor::DefaultElement<ElementType::IMAGE>,
      public DocumentCursor::ImageElement {
public:
  ImageElement(const OpenDocument *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  const ImageElement *image(const common::DocumentCursor &) const final {
    return this;
  }

  const char *href() const { return m_node.attribute("xlink:href").value(); }

  [[nodiscard]] bool
  internal(const common::DocumentCursor &cursor) const final {
    auto doc = document(cursor);

    if (!doc || !doc->files()) {
      return false;
    }

    try {
      const std::string href = this->href();
      const internal::common::Path path{href};

      return doc->files()->is_file(path);
    } catch (...) {
    }

    return false;
  }

  [[nodiscard]] std::optional<odr::File>
  image_file(const common::DocumentCursor &cursor) const final {
    auto doc = document(cursor);

    if (!doc) {
      // TODO throw; there is no "empty" file
      return File(std::shared_ptr<internal::abstract::File>());
    }

    if (!internal(cursor)) {
      // TODO throw / support external files
      return File(std::shared_ptr<internal::abstract::File>());
    }

    const std::string href = this->href();
    const internal::common::Path path{href};

    return File(doc->files()->open(path));
  }
};

DocumentCursor::Element *
construct_default_element(const OpenDocument *document, pugi::xml_node node,
                          DocumentCursor::Allocator allocator) {
  using Constructor = std::function<DocumentCursor::Element *(
      const OpenDocument *document, pugi::xml_node node,
      DocumentCursor::Allocator allocator)>;

  using Paragraph = DocumentCursor::DefaultElement<ElementType::PARAGRAPH>;
  using Span = DocumentCursor::DefaultElement<ElementType::SPAN>;
  using LineBreak = DocumentCursor::DefaultElement<ElementType::LINE_BREAK>;
  using Link = DocumentCursor::DefaultElement<ElementType::LINK>;
  using Bookmark = DocumentCursor::DefaultElement<ElementType::BOOKMARK>;
  using List = DocumentCursor::DefaultElement<ElementType::LIST>;
  using ListItem = DocumentCursor::DefaultElement<ElementType::LIST_ITEM>;
  using Group = DocumentCursor::DefaultElement<ElementType::GROUP>;
  using Frame = DocumentCursor::DefaultElement<ElementType::FRAME>;
  using Rect = DocumentCursor::DefaultElement<ElementType::RECT>;
  using Line = DocumentCursor::DefaultElement<ElementType::LINE>;
  using Circle = DocumentCursor::DefaultElement<ElementType::CIRCLE>;
  using CustomShape = DocumentCursor::DefaultElement<ElementType::CUSTOM_SHAPE>;

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

DocumentCursor::Element *
construct_default_parent_element(const OpenDocument *document,
                                 pugi::xml_node node,
                                 DocumentCursor::Allocator allocator) {
  for (pugi::xml_node parent = node.parent(); parent;
       parent = parent.parent()) {
    if (auto result = construct_default_element(document, parent, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
construct_default_first_child_element(const OpenDocument *document,
                                      pugi::xml_node node,
                                      DocumentCursor::Allocator allocator) {
  for (pugi::xml_node first_child = node.first_child(); first_child;
       first_child = first_child.next_sibling()) {
    if (auto result =
            construct_default_element(document, first_child, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *construct_default_previous_sibling_element(
    const OpenDocument *document, pugi::xml_node node,
    DocumentCursor::Allocator allocator) {
  for (pugi::xml_node previous_sibling = node.previous_sibling();
       previous_sibling;
       previous_sibling = previous_sibling.previous_sibling()) {
    if (auto result =
            construct_default_element(document, previous_sibling, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
construct_default_next_sibling_element(const OpenDocument *document,
                                       pugi::xml_node node,
                                       DocumentCursor::Allocator allocator) {
  for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
       next_sibling = next_sibling.next_sibling()) {
    if (auto result =
            construct_default_element(document, next_sibling, allocator)) {
      return result;
    }
  }
  return {};
}
} // namespace

DocumentCursor::DocumentCursor(const OpenDocument *document,
                               pugi::xml_node root)
    : m_document{document} {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = construct_default_element(document, root, allocator);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

const Style *DocumentCursor::style() const {
  if (m_document == nullptr) {
    return nullptr;
  }
  return &m_document->m_style;
}

} // namespace odr::internal::odf
