#include <cstring>
#include <functional>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_style.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
#include <string>
#include <utility>

namespace odr::internal::odf {

namespace {
const char *default_style_name(pugi::xml_node node);

std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store);

template <typename Derived>
std::tuple<abstract::Element *, pugi::xml_node> default_parse_element_tree(
    pugi::xml_node node,
    std::vector<std::unique_ptr<abstract::Element>> &store);
} // namespace

Element::Element() = default;

Element::Element(pugi::xml_node node) : common::Element(node) {}

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
Element::intermediate_style(const abstract::Document *document) const {
  if (m_parent == nullptr) {
    return partial_style(document);
  }
  auto base = dynamic_cast<Element *>(m_parent)->intermediate_style(document);
  base.override(partial_style(document));
  return base;
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
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  first_master_page(const abstract::Document *document) const final {
    return nullptr; // TODO fix
  }
};

class PresentationRoot final : public Root {
public:
  static constexpr auto child_name = "draw:page";

  using Root::Root;
};

class SpreadsheetRoot final : public Root {
public:
  static constexpr auto child_name = "table:table";

  using Root::Root;
};

class DrawingRoot final : public Root {
public:
  static constexpr auto child_name = "draw:page";

  using Root::Root;
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      // TODO
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

  [[nodiscard]] PageLayout
  page_layout(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      return MasterPage(master_page_node).page_layout(document);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  master_page(const abstract::Document *document) const final {
    if (auto master_page_node = master_page_node_(document)) {
      // TODO
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

  [[nodiscard]] TextStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).text_style;
  }
};

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] ParagraphStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).paragraph_style;
  }

  [[nodiscard]] TextStyle
  text_style(const abstract::Document *document) const final {
    return intermediate_style(document).text_style;
  }
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  [[nodiscard]] TextStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).text_style;
  }
};

class Text final : public Element, public abstract::TextElement {
public:
  using Element::Element;

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
  style(const abstract::Document *document) const final {
    return intermediate_style(document).text_style;
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

  [[nodiscard]] TextStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).text_style;
  }
};

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] TableStyle
  style(const abstract::Document *document) const final {
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

  abstract::Element *first_column(const abstract::Document *) const final {
    return {}; // TODO
  }

  abstract::Element *first_row(const abstract::Document *) const final {
    return {}; // TODO
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_column_style;
  }
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  [[nodiscard]] TableRowStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_row_style;
  }
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  column(const abstract::Document *) const final {
    return nullptr; // TODO
  }

  [[nodiscard]] abstract::Element *row(const abstract::Document *) const final {
    return nullptr; // TODO
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

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_cell_style;
  }
};

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

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
  style(const abstract::Document *document) const final {
    return intermediate_style(document).graphic_style;
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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).graphic_style;
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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).graphic_style;
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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).graphic_style;
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

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *document) const final {
    return intermediate_style(document).graphic_style;
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

  [[nodiscard]] abstract::Element *
  first_shape(const abstract::Document *) const final {
    return nullptr; // TODO
  }
};

std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store);

template <typename Derived>
std::tuple<abstract::Element *, pugi::xml_node> default_parse_element_tree(
    pugi::xml_node node,
    std::vector<std::unique_ptr<abstract::Element>> &store) {
  if (!node) {
    return std::make_tuple(nullptr, pugi::xml_node());
  }

  auto element_unique = std::make_unique<Derived>(node);
  auto element = dynamic_cast<abstract::Element *>(element_unique.get());
  store.push_back(std::move(element_unique));

  for (auto child_node : node) {
    auto [child, _] = parse_element_tree(child_node, store);
    if (child == nullptr) {
      continue;
    }

    // TODO attach child to root
  }

  return std::make_tuple(element, node.next_sibling());
}

std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store) {
  using Parser = std::function<std::tuple<abstract::Element *, pugi::xml_node>(
      pugi::xml_node node,
      std::vector<std::unique_ptr<abstract::Element>> & store)>;

  using List = DefaultElement<ElementType::list>;
  using Group = DefaultElement<ElementType::group>;
  using PageBreak = DefaultElement<ElementType::page_break>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"office:text", default_parse_element_tree<TextDocumentRoot>},
      {"office:presentation", default_parse_element_tree<PresentationRoot>},
      {"office:spreadsheet", default_parse_element_tree<SpreadsheetRoot>},
      {"office:drawing", default_parse_element_tree<DrawingRoot>},
      {"text:p", default_parse_element_tree<Paragraph>},
      {"text:h", default_parse_element_tree<Paragraph>},
      {"text:span", default_parse_element_tree<Span>},
      {"text:s", default_parse_element_tree<Text>},
      {"text:tab", default_parse_element_tree<Text>},
      {"text:line-break", default_parse_element_tree<LineBreak>},
      {"text:a", default_parse_element_tree<Link>},
      {"text:bookmark", default_parse_element_tree<Bookmark>},
      {"text:bookmark-start", default_parse_element_tree<Bookmark>},
      {"text:list", default_parse_element_tree<List>},
      {"text:list-header", default_parse_element_tree<ListItem>},
      {"text:list-item", default_parse_element_tree<ListItem>},
      {"text:index-title", default_parse_element_tree<Group>},
      {"text:table-of-content", default_parse_element_tree<Group>},
      {"text:illustration-index", default_parse_element_tree<Group>},
      {"text:index-body", default_parse_element_tree<Group>},
      {"text:soft-page-break", default_parse_element_tree<PageBreak>},
      {"text:date", default_parse_element_tree<Group>},
      {"text:time", default_parse_element_tree<Group>},
      {"text:section", default_parse_element_tree<Group>},
      //{"text:page-number", default_parse_element_tree<Group>},
      //{"text:page-continuation", default_parse_element_tree<Group>},
      {"table:table", default_parse_element_tree<TableElement>},
      {"table:table-column", default_parse_element_tree<TableColumn>},
      {"table:table-row", default_parse_element_tree<TableRow>},
      {"table:table-cell", default_parse_element_tree<TableCell>},
      {"table:covered-table-cell", default_parse_element_tree<TableCell>},
      {"draw:frame", default_parse_element_tree<Frame>},
      {"draw:image", default_parse_element_tree<ImageElement>},
      {"draw:rect", default_parse_element_tree<Rect>},
      {"draw:line", default_parse_element_tree<Line>},
      {"draw:circle", default_parse_element_tree<Circle>},
      {"draw:custom-shape", default_parse_element_tree<CustomShape>},
      {"draw:text-box", default_parse_element_tree<Group>},
      {"draw:g", default_parse_element_tree<Group>},
      {"draw:a", default_parse_element_tree<Link>},
  };

  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return default_parse_element_tree<Text>(node, store);
  }

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
odf::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<abstract::Element>> store;
  auto [root, _] = parse_element_tree(node, store);
  return std::make_tuple(root, store);
}

} // namespace odr::internal
