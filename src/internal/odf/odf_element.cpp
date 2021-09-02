#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_element.h>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::odf {

class Element : public abstract::Element {
protected:
  static const Document *document_(const abstract::Document *document) {
    return dynamic_cast<const Document *>(document);
  }

  static const StyleRegistry *style_(const abstract::Document *cursor) {
    return &dynamic_cast<const Document *>(cursor)->m_style_registry;
  }
};

namespace {

template <ElementType> class DefaultElement;
class DefaultRoot;
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

std::optional<std::string> default_style_name(const pugi::xml_node node) {
  for (auto attribute : node.attributes()) {
    if (util::string::ends_with(attribute.name(), ":style-name")) {
      return attribute.value();
    }
  }
  return {};
}

template <ElementType _element_type> class DefaultElement : public Element {
public:
  DefaultElement(const Document *, pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] bool equals(const abstract::Document *,
                            const abstract::Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *) const override {
    return default_style_name(m_node);
  }

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto name = style_name(document)) {
      return style_(document)->style(*name);
    }
    return nullptr;
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

  [[nodiscard]] const Link *link(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Bookmark *
  bookmark(const abstract::Document *) const override {
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

  [[nodiscard]] const Frame *frame(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Rect *rect(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Line *line(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Circle *
  circle(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const CustomShape *
  custom_shape(const abstract::Document *) const override {
    return nullptr;
  }

  [[nodiscard]] const Image *image(const abstract::Document *) const override {
    return nullptr;
  }

protected:
  pugi::xml_node m_node;
};

class MasterPage final : public DefaultElement<ElementType::master_page> {
public:
  MasterPage(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *) const override {
    if (auto attribute = m_node.attribute("style:page-layout-name")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto name = style_name(document)) {
      return style_(document)->page_layout(*name);
    }
    return nullptr;
  }
};

class DefaultRoot : public DefaultElement<ElementType::root> {
public:
  DefaultRoot(const Document *document, pugi::xml_node node)
      : DefaultElement<ElementType::root>(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const Allocator &) final {
    return nullptr;
  }
};

class TextDocumentRoot final : public DefaultRoot {
public:
  TextDocumentRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *document) const override {
    return style_(document)->first_master_page();
  }

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto first_master_page = style_(document)->first_master_page()) {
      auto master_page_node =
          style_(document)->master_page_node(*first_master_page);
      return MasterPage(document_(document), master_page_node).style(document);
    }
    return nullptr;
  }
};

class PresentationRoot final : public DefaultRoot {
public:
  PresentationRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<odf::Slide>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class SpreadsheetRoot final : public DefaultRoot {
public:
  SpreadsheetRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<Sheet>(
        document_(document), m_node.child("table:table"), allocator);
  }
};

class DrawingRoot final : public DefaultRoot {
public:
  DrawingRoot(const Document *document, pugi::xml_node node)
      : DefaultRoot(document, node) {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const Allocator &allocator) final {
    return construct_default_optional<Page>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class Slide final : public DefaultElement<ElementType::slide>,
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

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(document_(document), master_page_node).style(document);
    }
    return nullptr;
  }

  [[nodiscard]] const Slide *slide(const abstract::Document *) const final {
    return this;
  }

  abstract::Element *master_page(const abstract::Document *document,
                                 const abstract::Element *,
                                 const Allocator &allocator) const final {
    auto master_page_node = master_page_node_(document);
    return construct_default_optional<MasterPage>(document_(document),
                                                  master_page_node, allocator);
  }

private:
  pugi::xml_node master_page_node_(const abstract::Document *document) const {
    if (auto master_page_name_attr =
            m_node.attribute("draw:master-page-name")) {
      return style_(document)->master_page_node(master_page_name_attr.value());
    }
    return {};
  }
};

class Page final : public DefaultElement<ElementType::page> {
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

  [[nodiscard]] abstract::Style *
  style(const abstract::Document *document) const override {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(document_(document), master_page_node).style(document);
    }
    return nullptr;
  }

private:
  pugi::xml_node master_page_node_(const abstract::Document *document) const {
    if (auto master_page_name_attr =
            m_node.attribute("draw:master-page-name")) {
      return style_(document)->master_page_node(master_page_name_attr.value());
    }
    return {};
  }
};

class Text final : public DefaultElement<ElementType::text>,
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

class TableElement : public DefaultElement<ElementType::table>,
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

template <ElementType _element_type>
class DefaultTableElement : public DefaultElement<_element_type> {
public:
  DefaultTableElement(const Document *document, pugi::xml_node node)
      : DefaultElement<_element_type>(document, node) {}

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const Allocator &) final {
    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = previous_node_()) {
      DefaultElement<_element_type>::m_node = previous_sibling;
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
      DefaultElement<_element_type>::m_node = next_sibling;
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
    : public DefaultTableElement<ElementType::table_column> {
public:
  TableColumn(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node) {}

  [[nodiscard]] std::optional<std::string> default_cell_style_name() const {
    if (auto attribute = m_node.attribute("table:default-cell-style-name")) {
      return attribute.value();
    }
    return {};
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

class TableRow final : public DefaultTableElement<ElementType::table_row> {
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

class TableCell final : public DefaultTableElement<ElementType::table_cell>,
                        public abstract::Element::TableCell {
public:
  TableCell(const Document *document, pugi::xml_node node)
      : DefaultTableElement(document, node),
        m_column(document, node.parent().parent().child("table:table-column")) {
  }

  [[nodiscard]] std::optional<std::string>
  style_name(const abstract::Document *document) const final {
    if (auto name = DefaultElement::style_name(document)) {
      return name;
    }
    return m_column.default_cell_style_name();
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

class Frame final : public DefaultElement<ElementType::frame>,
                    public abstract::Element::Frame {
public:
  Frame(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const Frame *frame(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::optional<std::string>
  anchor_type(const abstract::Document *,
              const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("text:anchor-type")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *, const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("svg:x")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *, const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("svg:y")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *, const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("svg:width")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *, const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("svg:height")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *, const abstract::Element *) const final {
    if (auto attribute = m_node.attribute("draw:z-index")) {
      return attribute.value();
    }
    return {};
  }
};

class Rect final : public DefaultElement<ElementType::rect>,
                   public abstract::Element::Rect {
public:
  Rect(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const Rect *rect(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::string x(const abstract::Document *,
                              const abstract::Element *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::string y(const abstract::Document *,
                              const abstract::Element *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *,
                                  const abstract::Element *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *,
                                   const abstract::Element *) const final {
    return m_node.attribute("svg:height").value();
  }
};

class Line final : public DefaultElement<ElementType::line>,
                   public abstract::Element::Line {
public:
  Line(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const Line *line(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::string x1(const abstract::Document *,
                               const abstract::Element *) const final {
    return m_node.attribute("svg:x1").value();
  }

  [[nodiscard]] std::string y1(const abstract::Document *,
                               const abstract::Element *) const final {
    return m_node.attribute("svg:y1").value();
  }

  [[nodiscard]] std::string x2(const abstract::Document *,
                               const abstract::Element *) const final {
    return m_node.attribute("svg:x2").value();
  }

  [[nodiscard]] std::string y2(const abstract::Document *,
                               const abstract::Element *) const final {
    return m_node.attribute("svg:y2").value();
  }
};

class Circle final : public DefaultElement<ElementType::circle>,
                     public abstract::Element::Circle {
public:
  Circle(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const Circle *circle(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::string x(const abstract::Document *,
                              const abstract::Element *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::string y(const abstract::Document *,
                              const abstract::Element *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *,
                                  const abstract::Element *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *,
                                   const abstract::Element *) const final {
    return m_node.attribute("svg:height").value();
  }
};

class CustomShape final : public DefaultElement<ElementType::custom_shape>,
                          public abstract::Element::CustomShape {
public:
  CustomShape(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] const CustomShape *
  custom_shape(const abstract::Document *) const final {
    return this;
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *, const abstract::Element *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *, const abstract::Element *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *,
                                  const abstract::Element *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *,
                                   const abstract::Element *) const final {
    return m_node.attribute("svg:height").value();
  }
};

class ImageElement final : public DefaultElement<ElementType::image>,
                           public abstract::Element::Image {
public:
  ImageElement(const Document *document, pugi::xml_node node)
      : DefaultElement(document, node) {}

  [[nodiscard]] bool internal(const abstract::Document *document,
                              const abstract::Element *element) const final {
    auto doc = document_(document);
    if (!doc || !doc->files()) {
      return false;
    }
    try {
      return doc->files()->is_file(this->href(document, element));
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
    return File(doc->files()->open(this->href(document, element)));
  }

  [[nodiscard]] std::string href(const abstract::Document *,
                                 const abstract::Element *) const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Sheet final : public TableElement {
public:
  Sheet(const Document *document, pugi::xml_node node)
      : TableElement(document, node) {}

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return ElementType::sheet;
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

  using Paragraph = DefaultElement<ElementType::paragraph>;
  using Span = DefaultElement<ElementType::span>;
  using LineBreak = DefaultElement<ElementType::line_break>;
  using Link = DefaultElement<ElementType::link>;
  using Bookmark = DefaultElement<ElementType::bookmark>;
  using List = DefaultElement<ElementType::list>;
  using ListItem = DefaultElement<ElementType::list_item>;
  using Group = DefaultElement<ElementType::group>;

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

} // namespace odr::internal
