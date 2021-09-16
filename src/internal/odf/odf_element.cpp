#include <cstring>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_element.h>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::odf {

namespace {
const char *default_style_name(pugi::xml_node node);
}

Element::Element(const Document *, pugi::xml_node node) : m_node{node} {}

bool Element::equals(const abstract::Document *,
                     const abstract::DocumentCursor *,
                     const abstract::Element &rhs) const {
  return m_node == *dynamic_cast<const Element &>(rhs).m_node;
}

abstract::Element *Element::parent(const abstract::Document *document,
                                   const abstract::DocumentCursor *,
                                   const abstract::Allocator *allocator) {
  return construct_default_parent_element(document_(document), m_node,
                                          allocator);
}

abstract::Element *Element::first_child(const abstract::Document *document,
                                        const abstract::DocumentCursor *,
                                        const abstract::Allocator *allocator) {
  return construct_default_first_child_element(document_(document), m_node,
                                               allocator);
}

abstract::Element *
Element::previous_sibling(const abstract::Document *document,
                          const abstract::DocumentCursor *,
                          const abstract::Allocator *allocator) {
  return construct_default_previous_sibling_element(document_(document), m_node,
                                                    allocator);
}

abstract::Element *Element::next_sibling(const abstract::Document *document,
                                         const abstract::DocumentCursor *,
                                         const abstract::Allocator *allocator) {
  return construct_default_next_sibling_element(document_(document), m_node,
                                                allocator);
}

common::ResolvedStyle
Element::partial_style(const abstract::Document *document) const {
  if (auto style_name = style_name_(document)) {
    if (auto style = style_(document)->style(style_name)) {
      return style->resolved();
    }
  }
  return {};
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *,
                            const abstract::DocumentCursor *cursor) const {
  return static_cast<const DocumentCursor *>(cursor)->intermediate_style();
}

const char *Element::style_name_(const abstract::Document *) const {
  return default_style_name(m_node);
}

const Document *Element::document_(const abstract::Document *document) {
  return static_cast<const Document *>(document);
}

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &static_cast<const Document *>(document)->m_style_registry;
}

namespace {

template <ElementType> class DefaultElement;
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
                                     const abstract::Allocator *allocator) {
  auto alloc = (*allocator)(sizeof(Derived));
  return new (alloc) Derived(document, node);
}

template <typename Derived>
abstract::Element *
construct_default_optional(const Document *document, pugi::xml_node node,
                           const abstract::Allocator *allocator) {
  if (!node) {
    return nullptr;
  }
  return construct_default<Derived>(document, node, allocator);
}

const char *default_style_name(const pugi::xml_node node) {
  for (auto attribute : node.attributes()) {
    if (util::string::ends_with(attribute.name(), ":style-name")) {
      return attribute.value();
    }
  }
  return {};
}

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return element_type;
  }
};

class MasterPage final : public Element, public abstract::MasterPageElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto attribute = m_node.attribute("style:page-layout-name")) {
      return style_(document)->page_layout(attribute.value());
    }
    return {};
  }
};

class Root : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return ElementType::root;
  }

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    return nullptr;
  }
};

class TextDocumentRoot final : public Root, public abstract::TextRootElement {
public:
  using Root::Root;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::root;
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto first_master_page = style_(document)->first_master_page()) {
      auto master_page_node =
          style_(document)->master_page_node(*first_master_page);
      return MasterPage(document_(document), master_page_node)
          .page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  first_master_page(const abstract::Document *document,
                    const abstract::DocumentCursor *,
                    const abstract::Allocator *allocator) const final {
    if (auto first_master_page = style_(document)->first_master_page()) {
      auto master_page_node =
          style_(document)->master_page_node(*first_master_page);
      return construct_default_optional<MasterPage>(
          document_(document), master_page_node, allocator);
    }
    return nullptr;
  }
};

class PresentationRoot final : public Root {
public:
  using Root::Root;

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_optional<Slide>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_optional<Sheet>(
        document_(document), m_node.child("table:table"), allocator);
  }
};

class DrawingRoot final : public Root {
public:
  using Root::Root;

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_optional<Page>(
        document_(document), m_node.child("draw:page"), allocator);
  }
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("draw:page")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("draw:page")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(document_(document), master_page_node)
          .page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document,
              const abstract::DocumentCursor *,
              const abstract::Allocator *allocator) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return construct_default_optional<MasterPage>(
          document_(document), master_page_node, allocator);
    }
    return {};
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("draw:name").value();
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

class Page final : public Element, public abstract::PageElement {
public:
  using Element::Element;

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("draw:page")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("draw:page")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(document_(document), master_page_node)
          .page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document,
              const abstract::DocumentCursor *,
              const abstract::Allocator *allocator) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return construct_default_optional<MasterPage>(
          document_(document), master_page_node, allocator);
    }
    return {};
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("draw:name").value();
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

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<ParagraphStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).paragraph_style;
  }
};

class Text final : public Element, public abstract::TextElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<TextStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }

  abstract::Element *
  previous_sibling(const abstract::Document *document,
                   const abstract::DocumentCursor *,
                   const abstract::Allocator *allocator) final {
    return construct_default_previous_sibling_element(document_(document),
                                                      first_(), allocator);
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *allocator) final {
    return construct_default_next_sibling_element(document_(document), last_(),
                                                  allocator);
  }

  [[nodiscard]] std::string value(const abstract::Document *) const final {
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

class Link final : public Element, public abstract::LinkElement {
public:
  using Element::Element;

  [[nodiscard]] std::string href(const abstract::Document *) const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public abstract::BookmarkElement {
public:
  using Element::Element;

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("text:name").value();
  }
};

class ListItem final : public Element, public abstract::ListItemElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<TextStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }
};

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<TableStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_style;
  }

  abstract::Element *first_child(const abstract::Document *,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *) final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final {
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

  abstract::Element *
  first_column(const abstract::Document *document,
               const abstract::DocumentCursor *,
               const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableColumn>(
        document_(document), m_node.child("table:table-column"), allocator);
  }

  abstract::Element *
  first_row(const abstract::Document *document,
            const abstract::DocumentCursor *,
            const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableRow>(
        document_(document), m_node.child("table:table-row"), allocator);
  }
};

class TableComponent : public Element {
public:
  using Element::Element;

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) override {
    if (m_repeated_index > 0) {
      --m_repeated_index;
      return this;
    }

    if (auto previous_sibling = previous_node_()) {
      m_node = previous_sibling;
      m_repeated_index = 0;
      return this;
    }

    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) override {
    if (m_repeated_index < number_repeated_() - 1) {
      ++m_repeated_index;
      return this;
    }

    if (auto next_sibling = next_node_()) {
      m_node = next_sibling;
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

class TableColumn final : public TableComponent,
                          public abstract::TableColumnElement {
public:
  using TableComponent::TableComponent;

  [[nodiscard]] std::optional<TableColumnStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_column_style;
  }

  common::ResolvedStyle
  default_cell_style(const abstract::Document *document) const {
    if (auto attribute = m_node.attribute("table:default-cell-style-name")) {
      if (auto style = style_(document)->style(attribute.value())) {
        return style->resolved();
      }
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

class TableRow final : public TableComponent, public abstract::TableRowElement {
public:
  using TableComponent::TableComponent;

  [[nodiscard]] std::optional<TableRowStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_row_style;
  }

  common::ResolvedStyle
  default_cell_style(const abstract::Document *document) const {
    if (auto attribute = m_node.attribute("table:default-cell-style-name")) {
      if (auto style = style_(document)->style(attribute.value())) {
        return style->resolved();
      }
    }
    return {};
  }

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

class TableCell final : public TableComponent,
                        public abstract::TableCellElement {
public:
  // TODO this only works for first cells
  TableCell(const Document *document, pugi::xml_node node)
      : TableComponent(document, node), m_column{document,
                                                 node.parent().parent().child(
                                                     "table:table-column")},
        m_row{document, node.parent()} {}

  abstract::Element *
  previous_sibling(const abstract::Document *document,
                   const abstract::DocumentCursor *cursor,
                   const abstract::Allocator *allocator) final {
    m_column.previous_sibling(document, cursor, nullptr);
    return TableComponent::next_sibling(document, cursor, allocator);
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const abstract::DocumentCursor *cursor,
                                  const abstract::Allocator *allocator) final {
    m_column.next_sibling(document, cursor, nullptr);
    return TableComponent::next_sibling(document, cursor, allocator);
  }

  [[nodiscard]] const abstract::Element *
  column(const abstract::Document *,
         const abstract::DocumentCursor *) const final {
    return &m_column;
  }

  [[nodiscard]] const abstract::Element *
  row(const abstract::Document *,
      const abstract::DocumentCursor *) const final {
    return &m_row;
  }

  [[nodiscard]] bool covered(const abstract::Document *) const final {
    return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    common::ResolvedStyle result;
    result.override(m_column.default_cell_style(document));
    result.override(m_row.default_cell_style(document));
    result.override(Element::partial_style(document));
    return result;
  }

  [[nodiscard]] std::optional<TableCellStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_cell_style;
  }

private:
  TableColumn m_column;
  TableRow m_row;

  const char *style_name_(const abstract::Document *) const final {
    if (auto style_name = m_node.attribute("table:style-name")) {
      return style_name.value();
    }
    return {};
  }

  [[nodiscard]] std::uint32_t number_repeated_() const final {
    return m_node.attribute("table:number-columns-repeated").as_uint(1);
  }

  [[nodiscard]] pugi::xml_node previous_node_() const final {
    return m_node.previous_sibling();
  }

  [[nodiscard]] pugi::xml_node next_node_() const final {
    return m_node.next_sibling();
  }
};

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<std::string>
  anchor_type(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("text:anchor-type")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("svg:x")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("svg:y")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("svg:width")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("svg:height")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final {
    if (auto attribute = m_node.attribute("draw:z-index")) {
      return attribute.value();
    }
    return {};
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Rect final : public Element, public abstract::RectElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x(const abstract::Document *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::string y(const abstract::Document *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *) const final {
    return m_node.attribute("svg:height").value();
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Line final : public Element, public abstract::LineElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x1(const abstract::Document *) const final {
    return m_node.attribute("svg:x1").value();
  }

  [[nodiscard]] std::string y1(const abstract::Document *) const final {
    return m_node.attribute("svg:y1").value();
  }

  [[nodiscard]] std::string x2(const abstract::Document *) const final {
    return m_node.attribute("svg:x2").value();
  }

  [[nodiscard]] std::string y2(const abstract::Document *) const final {
    return m_node.attribute("svg:y2").value();
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Circle final : public Element, public abstract::CircleElement {
public:
  using Element::Element;

  [[nodiscard]] std::string x(const abstract::Document *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::string y(const abstract::Document *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *) const final {
    return m_node.attribute("svg:height").value();
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class CustomShape final : public Element, public abstract::CustomShapeElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    return m_node.attribute("svg:x").value();
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    return m_node.attribute("svg:y").value();
  }

  [[nodiscard]] std::string width(const abstract::Document *) const final {
    return m_node.attribute("svg:width").value();
  }

  [[nodiscard]] std::string height(const abstract::Document *) const final {
    return m_node.attribute("svg:height").value();
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *document) const final {
    auto doc = document_(document);
    if (!doc || !doc->files()) {
      return false;
    }
    try {
      return doc->files()->is_file(href(document));
    } catch (...) {
    }
    return false;
  }

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *document) const final {
    auto doc = document_(document);
    if (!doc || !internal(document)) {
      return {};
    }
    return File(doc->files()->open(href(document)));
  }

  [[nodiscard]] std::string href(const abstract::Document *) const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Sheet final : public TableElement, public abstract::SheetElement {
public:
  using TableElement::TableElement;

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("table:name").value();
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return ElementType::sheet;
  }

  Element *previous_sibling(const abstract::Document *,
                            const abstract::DocumentCursor *,
                            const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("table:table")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  Element *next_sibling(const abstract::Document *,
                        const abstract::DocumentCursor *,
                        const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("table:table")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  content(const abstract::Document *,
          const std::optional<TableDimensions> range) const final {
    TableDimensions result;

    common::TableCursor cursor;
    for (auto row : m_node.children("table:table-row")) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);

      for (auto cell : row.children("table:table-cell")) {
        const auto columns_repeated =
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const auto colspan =
            cell.attribute("table:number-columns-spanned").as_uint(1);
        const auto rowspan =
            cell.attribute("table:number-rows-spanned").as_uint(1);
        cursor.add_cell(colspan, rowspan, columns_repeated);

        const auto new_rows = cursor.row();
        const auto new_cols = std::max(result.columns, cursor.column());
        if (cell.first_child() && (range && (new_rows < range->rows) &&
                                   (new_cols < range->columns))) {
          result.rows = new_rows;
          result.columns = new_cols;
        }
      }
    }

    return result;
  }
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

abstract::Element *
odf::construct_default_element(const Document *document, pugi::xml_node node,
                               const abstract::Allocator *allocator) {
  using Constructor = std::function<abstract::Element *(
      const Document *document, pugi::xml_node node,
      const abstract::Allocator *allocator)>;

  using Span = DefaultElement<ElementType::span>;
  using LineBreak = DefaultElement<ElementType::line_break>;
  using List = DefaultElement<ElementType::list>;
  using Group = DefaultElement<ElementType::group>;
  using PageBreak = DefaultElement<ElementType::page_break>;

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
      {"text:index-title", construct_default<Group>},
      {"text:table-of-content", construct_default<Group>},
      {"text:index-body", construct_default<Group>},
      {"text:soft-page-break", construct_default<PageBreak>},
      {"text:date", construct_default<Group>},
      {"text:time", construct_default<Group>},
      {"text:page-number", construct_default<Group>},
      {"text:page-continuation", construct_default<Group>},
      {"table:table", construct_default<TableElement>},
      {"table:table-column", construct_default<TableColumn>},
      {"table:table-row", construct_default<TableRow>},
      {"table:table-cell", construct_default<TableCell>},
      {"table:covered-table-cell", construct_default<TableCell>},
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

abstract::Element *
odf::construct_default_parent_element(const Document *document,
                                      pugi::xml_node node,
                                      const abstract::Allocator *allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_first_child_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *odf::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

} // namespace odr::internal
