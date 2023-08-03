#include <odr/internal/odf/odf_element.hpp>

#include <odr/file.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <pugixml.hpp>

#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace odr::internal::odf {

namespace {
const char *default_style_name(pugi::xml_node node);
}

Element::Element() = default;

Element::Element(pugi::xml_node node) : common::Element<Element>(node) {}

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

class Slide;
class Sheet;
class Page;
class TableColumn;
class TableRow;

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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const override {
    return common::construct_2<DefaultElement>(*this);
  }
};

class MasterPage final : public Element, public abstract::MasterPageElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<MasterPage>(*this);
  }

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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const override {
    return common::construct_2<Root>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return {};
  }
};

class TextDocumentRoot final : public Root, public abstract::TextRootElement {
public:
  using Root::Root;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::root;
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TextDocumentRoot>(*this);
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto first_master_page = style_(document)->first_master_page()) {
      auto master_page_node =
          style_(document)->master_page_node(*first_master_page);
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  first_master_page(const abstract::Document *document,
                    const abstract::DocumentCursor *) const final {
    if (auto first_master_page = style_(document)->first_master_page()) {
      auto master_page_node =
          style_(document)->master_page_node(*first_master_page);
      return common::construct_optional<MasterPage>(master_page_node);
    }
    return {};
  }
};

class PresentationRoot final : public Root {
public:
  using Root::Root;

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_optional<Slide>(m_node.child("draw:page"));
  }
};

class SpreadsheetRoot final : public Root {
public:
  using Root::Root;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<SpreadsheetRoot>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(m_node.child("table:table"));
  }
};

class DrawingRoot final : public Root {
public:
  using Root::Root;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<DrawingRoot>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_optional<Page>(m_node.child("draw:page"));
  }
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Slide>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<Slide>(
        m_node.previous_sibling("draw:page"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<Slide>(m_node.next_sibling("draw:page"));
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_master_page(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return common::construct_optional<MasterPage>(master_page_node);
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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Page>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<Page>(
        m_node.previous_sibling("draw:page"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<Page>(m_node.next_sibling("draw:page"));
  }

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  master_page(const abstract::Document *document,
              const abstract::DocumentCursor *) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return common::construct_optional<MasterPage>(master_page_node);
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

class LineBreak final : public Element, public abstract::LineBreakElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<LineBreak>(*this);
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }
};

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Paragraph>(*this);
  }

  [[nodiscard]] ParagraphStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).paragraph_style;
  }

  [[nodiscard]] TextStyle
  text_style(const abstract::Document *document,
             const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Span>(*this);
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }
};

class Text final : public Element, public abstract::TextElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Text>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_previous_sibling_element(construct_default_element,
                                                      first_());
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_next_sibling_element(construct_default_element,
                                                  last_());
  }

  [[nodiscard]] std::string content(const abstract::Document *) const final {
    std::string result;
    for (auto node = first_(); is_text_(node); node = node.next_sibling()) {
      result += text_(node);
    }
    return result;
  }

  void set_content(const abstract::Document *, const std::string &text) final {
    auto parent = m_node.parent();
    auto old_start = m_node;

    auto tokens = util::xml::tokenize_text(text);
    for (auto &&token : tokens) {
      switch (token.type) {
      case util::xml::StringToken::Type::none:
        break;
      case util::xml::StringToken::Type::string: {
        auto text_node = parent.insert_child_before(
            pugi::xml_node_type::node_pcdata, old_start);
        text_node.text().set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::spaces: {
        auto space_node = parent.insert_child_before("text:s", old_start);
        space_node.prepend_attribute("text:c").set_value(token.string.size());
      } break;
      case util::xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          parent.insert_child_before("text:tab", old_start);
        }
      } break;
      }
    }

    m_node = first_();
    auto new_end = old_start.previous_sibling();

    while (is_text_(new_end.next_sibling())) {
      parent.remove_child(new_end.next_sibling());
    }
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Link>(*this);
  }

  [[nodiscard]] std::string href(const abstract::Document *) const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public abstract::BookmarkElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Bookmark>(*this);
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("text:name").value();
  }
};

class ListItem final : public Element, public abstract::ListItemElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<ListItem>(*this);
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }
};

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const override {
    return common::construct_2<TableElement>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] TableStyle style(const abstract::Document *document,
                                 const abstract::DocumentCursor *) const final {
    return partial_style(document).table_style;
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

  std::unique_ptr<abstract::Element>
  construct_first_column(const abstract::Document *) const final {
    return common::construct_optional<TableColumn>(
        m_node.child("table:table-column"));
  }

  std::unique_ptr<abstract::Element>
  construct_first_row(const abstract::Document *) const final {
    return common::construct_optional<TableRow>(
        m_node.child("table:table-row"));
  }
};

template <typename Derived> class TableComponent : public Element {
public:
  using Element::Element;

  TableComponent(pugi::xml_node node, std::uint32_t repeated_index)
      : Element(node), m_repeated_index{repeated_index} {}

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const override {
    if (m_repeated_index > 0) {
      return common::construct_2<Derived>(m_node, m_repeated_index - 1);
    }
    if (auto previous_sibling = previous_node_()) {
      // TODO not 0 but last repeated
      return common::construct_2<Derived>(previous_sibling, 0);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const override {
    if (m_repeated_index < number_repeated_() - 1) {
      return common::construct_2<Derived>(m_node, m_repeated_index + 1);
    }
    if (auto next_sibling = next_node_()) {
      return common::construct_2<Derived>(next_sibling, 0);
    }
    return {};
  }

protected:
  std::uint32_t m_repeated_index{0};

  [[nodiscard]] virtual std::uint32_t number_repeated_() const = 0;
  [[nodiscard]] virtual pugi::xml_node previous_node_() const = 0;
  [[nodiscard]] virtual pugi::xml_node next_node_() const = 0;
};

class TableColumn final : public TableComponent<TableColumn>,
                          public abstract::TableColumnElement {
public:
  using TableComponent::TableComponent;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableColumn>(*this);
  }

  [[nodiscard]] TableColumnStyle
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

class TableRow final : public TableComponent<TableRow>,
                       public abstract::TableRowElement {
public:
  using TableComponent::TableComponent;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableRow>(*this);
  }

  [[nodiscard]] TableRowStyle
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

class TableCell final : public TableComponent<TableCell>,
                        public abstract::TableCellElement {
public:
  using TableComponent::TableComponent;

  // TODO this only works for first cells
  explicit TableCell(pugi::xml_node node)
      : TableComponent(node), m_column{node.parent().parent().child(
                                  "table:table-column")},
        m_row{node.parent()} {}
  TableCell(pugi::xml_node node, std::uint32_t repeated_index,
            TableColumn column)
      : TableComponent(node, repeated_index), m_column{std::move(column)},
        m_row{node.parent()} {}

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableCell>(*this);
  }

  std::unique_ptr<abstract::Element> construct_previous_sibling(
      const abstract::Document *document) const override {
    TableColumn previous_column;
    if (auto previous_column_ptr =
            m_column.construct_previous_sibling(document)) {
      previous_column = dynamic_cast<const TableColumn &>(*previous_column_ptr);
    }
    if (m_repeated_index > 0) {
      return common::construct_2<TableCell>(m_node, m_repeated_index - 1,
                                            previous_column);
    }
    if (auto previous_sibling = previous_node_()) {
      // TODO not 0 but last repeated
      return common::construct_2<TableCell>(previous_sibling, 0,
                                            previous_column);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *document) const override {
    TableColumn next_column;
    if (auto next_column_ptr = m_column.construct_next_sibling(document)) {
      next_column = dynamic_cast<const TableColumn &>(*next_column_ptr);
    }
    if (m_repeated_index < number_repeated_() - 1) {
      return common::construct_2<TableCell>(m_node, m_repeated_index + 1,
                                            next_column);
    }
    if (auto next_sibling = next_node_()) {
      return common::construct_2<TableCell>(next_sibling, 0, next_column);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  column(const abstract::Document *, const abstract::DocumentCursor *) final {
    return &m_column;
  }

  [[nodiscard]] abstract::Element *row(const abstract::Document *,
                                       const abstract::DocumentCursor *) final {
    return &m_row;
  }

  [[nodiscard]] bool covered(const abstract::Document *) const final {
    return std::strcmp(m_node.name(), "table:covered-table-cell") == 0;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {m_node.attribute("table:number-rows-spanned").as_uint(1),
            m_node.attribute("table:number-columns-spanned").as_uint(1)};
  }

  [[nodiscard]] ValueType value_type(const abstract::Document *) const final {
    auto value_type = m_node.attribute("office:value-type").value();
    if (std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    common::ResolvedStyle result;
    result.override(m_column.default_cell_style(document));
    result.override(m_row.default_cell_style(document));
    result.override(Element::partial_style(document));
    return result;
  }

  [[nodiscard]] TableCellStyle
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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Frame>(*this);
  }

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *) const final {
    auto anchor_type = m_node.attribute("text:anchor-type").value();
    if (std::strcmp("as-char", anchor_type) == 0) {
      return AnchorType::as_char;
    }
    if (std::strcmp("char", anchor_type) == 0) {
      return AnchorType::at_char;
    }
    if (std::strcmp("paragraph", anchor_type) == 0) {
      return AnchorType::at_paragraph;
    }
    if (std::strcmp("page", anchor_type) == 0) {
      return AnchorType::at_page;
    }
    return AnchorType::at_page;
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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Rect final : public Element, public abstract::RectElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Rect>(*this);
  }

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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Line final : public Element, public abstract::LineElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Line>(*this);
  }

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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class Circle final : public Element, public abstract::CircleElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Circle>(*this);
  }

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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class CustomShape final : public Element, public abstract::CustomShapeElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<CustomShape>(*this);
  }

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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).graphic_style;
  }
};

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<ImageElement>(*this);
  }

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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Sheet>(*this);
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(
        m_node.previous_sibling("table:table"));
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(
        m_node.next_sibling("table:table"));
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("table:name").value();
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return ElementType::sheet;
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

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_first_shape(const abstract::Document *) const final {
    return common::construct_first_child_element(construct_default_element,
                                                 m_node.child("table:shapes"));
  }
};

} // namespace

std::unique_ptr<abstract::Element>
Element::construct_default_element(pugi::xml_node node) {
  using Constructor =
      std::function<std::unique_ptr<abstract::Element>(pugi::xml_node node)>;

  using List = DefaultElement<ElementType::list>;
  using Group = DefaultElement<ElementType::group>;
  using PageBreak = DefaultElement<ElementType::page_break>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"office:text", common::construct<TextDocumentRoot>},
      {"office:presentation", common::construct<PresentationRoot>},
      {"office:spreadsheet", common::construct<SpreadsheetRoot>},
      {"office:drawing", common::construct<DrawingRoot>},
      {"text:p", common::construct<Paragraph>},
      {"text:h", common::construct<Paragraph>},
      {"text:span", common::construct<Span>},
      {"text:s", common::construct<Text>},
      {"text:tab", common::construct<Text>},
      {"text:line-break", common::construct<LineBreak>},
      {"text:a", common::construct<Link>},
      {"text:bookmark", common::construct<Bookmark>},
      {"text:bookmark-start", common::construct<Bookmark>},
      {"text:list", common::construct<List>},
      {"text:list-header", common::construct<ListItem>},
      {"text:list-item", common::construct<ListItem>},
      {"text:index-title", common::construct<Group>},
      {"text:table-of-content", common::construct<Group>},
      {"text:illustration-index", common::construct<Group>},
      {"text:index-body", common::construct<Group>},
      {"text:soft-page-break", common::construct<PageBreak>},
      {"text:date", common::construct<Group>},
      {"text:time", common::construct<Group>},
      {"text:section", common::construct<Group>},
      //{"text:page-number", common::construct<Group>},
      //{"text:page-continuation", common::construct<Group>},
      {"table:table", common::construct<TableElement>},
      {"table:table-column", common::construct<TableColumn>},
      {"table:table-row", common::construct<TableRow>},
      {"table:table-cell", common::construct<TableCell>},
      {"table:covered-table-cell", common::construct<TableCell>},
      {"draw:frame", common::construct<Frame>},
      {"draw:image", common::construct<ImageElement>},
      {"draw:rect", common::construct<Rect>},
      {"draw:line", common::construct<Line>},
      {"draw:circle", common::construct<Circle>},
      {"draw:custom-shape", common::construct<CustomShape>},
      {"draw:text-box", common::construct<Group>},
      {"draw:g", common::construct<Group>},
      {"draw:a", common::construct<Link>},
  };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return common::construct<Text>(node);
  }

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node);
  }

  return {};
}

} // namespace odr::internal::odf
