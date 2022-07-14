#include <functional>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
#include <unordered_map>
#include <utility>

namespace odr::internal::ooxml::spreadsheet {

Element::Element() = default;

Element::Element(pugi::xml_node node) : common::Element<Element>(node) {}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {};
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *,
                            const abstract::DocumentCursor *cursor) const {
  return static_cast<const DocumentCursor *>(cursor)->intermediate_style();
}

const Document *Element::document_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document);
}

const StyleRegistry *
Element::style_registry_(const abstract::Document *document) {
  return &document_(document)->m_style_registry;
}

pugi::xml_node Element::sheet_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_sheets.at(id).sheet_xml.document_element();
}

pugi::xml_node Element::drawing_(const abstract::Document *document,
                                 const std::string &id) {
  return document_(document)->m_sheets.at(id).drawing_xml.document_element();
}

const std::vector<pugi::xml_node> &
Element::shared_strings_(const abstract::Document *document) {
  return document_(document)->m_shared_strings;
}

namespace {

class Sheet;
class TableColumn;
class TableRow;
class TableCell;
class Text;
class ImageElement;

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return element_type;
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const override {
    return common::construct_2<DefaultElement>(*this);
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Root>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(
        m_node.child("sheets").child("sheet"));
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

class Sheet final : public Element,
                    public abstract::SheetElement,
                    public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Sheet>(*this);
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::sheet;
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(m_node.previous_sibling("sheet"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<Sheet>(m_node.next_sibling("sheet"));
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return m_node.attribute("name").value();
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *document) const final {
    if (auto dimension =
            sheet_node_(document).child("dimension").attribute("ref")) {
      try {
        auto range = common::TableRange(dimension.value());
        return {range.to().row() + 1, range.to().column() + 1};
      } catch (...) {
      }
    }
    return {};
  }

  [[nodiscard]] TableDimensions
  content(const abstract::Document *document,
          std::optional<TableDimensions>) const final {
    return dimensions(document); // TODO
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_first_column(const abstract::Document *document) const final {
    return common::construct_optional<TableColumn>(
        sheet_node_(document).child("cols").child("col"));
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_first_row(const abstract::Document *document) const final {
    return common::construct_optional<TableRow>(
        sheet_node_(document).child("sheetData").child("row"));
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_first_shape(const abstract::Document *document) const final {
    return common::construct_first_child_element(construct_default_element,
                                                 drawing_node_(document));
  }

  [[nodiscard]] TableStyle style(const abstract::Document *document,
                                 const abstract::DocumentCursor *) const final {
    return partial_style(document).table_style;
  }

private:
  pugi::xml_node sheet_node_(const abstract::Document *document) const {
    return sheet_(document, m_node.attribute("r:id").value());
  }

  pugi::xml_node drawing_node_(const abstract::Document *document) const {
    return drawing_(document, m_node.attribute("r:id").value());
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  TableColumn(pugi::xml_node node, std::uint32_t column)
      : Element(node), m_column{column} {}

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableColumn>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    if (min_() <= m_column - 1) {
      return common::construct_2<TableColumn>(m_node, m_column - 1);
    }
    if (auto previous_sibling = m_node.previous_sibling()) {
      return common::construct_2<TableColumn>(previous_sibling, m_column - 1);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    if (max_() >= m_column + 1) {
      return common::construct_2<TableColumn>(m_node, m_column + 1);
    }
    if (auto next_sibling = m_node.next_sibling()) {
      return common::construct_2<TableColumn>(next_sibling, m_column + 1);
    }
    return {};
  }

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    TableColumnStyle result;
    if (auto width = m_node.attribute("width")) {
      result.width = Measure(width.as_float(), DynamicUnit("ch"));
    }
    return result;
  }

private:
  std::uint32_t m_column{0};

  [[nodiscard]] std::uint32_t min_() const {
    return m_node.attribute("min").as_uint() - 1;
  }
  [[nodiscard]] std::uint32_t max_() const {
    return m_node.attribute("min").as_uint() - 1;
  }

  friend class TableCell;
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  TableRow(pugi::xml_node node, std::uint32_t row)
      : Element(node), m_row{row} {}

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableRow>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    if (skip_()) {
      return {};
    }
    if (auto first_child = m_node.child("c")) {
      return common::construct_2<TableCell>(
          first_child, *this,
          TableColumn(m_node.parent().parent().child("cols").child("col")));
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    if (m_row <= 0) {
      return {};
    }
    auto previous_row = m_row - 1;
    if (node_position_(m_node) <= previous_row) {
      return common::construct_2<TableRow>(m_node, previous_row);
    }
    if (auto next_sibling = m_node.next_sibling("row")) {
      return common::construct_2<TableRow>(next_sibling, previous_row);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    auto next_row = m_row + 1;
    if (node_position_(m_node) >= next_row) {
      return common::construct_2<TableRow>(m_node, next_row);
    }
    if (auto next_sibling = m_node.next_sibling("row")) {
      return common::construct_2<TableRow>(next_sibling, next_row);
    }
    return {};
  }

  [[nodiscard]] TableRowStyle
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    TableRowStyle result;
    if (auto height = m_node.attribute("ht")) {
      result.height = Measure(height.as_float(), DynamicUnit("pt"));
    }
    return result;
  }

private:
  std::uint32_t m_row{0};

  [[nodiscard]] static std::uint32_t node_position_(pugi::xml_node node) {
    return node.attribute("r").as_ullong() - 1;
  }

  [[nodiscard]] bool skip_() const {
    return !m_node || m_row != node_position_(m_node);
  }

  friend class TableCell;
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  // TODO remove
  using Element::Element;

  TableCell(pugi::xml_node node, TableRow row, TableColumn column)
      : Element(node), m_row{std::move(row)}, m_column{std::move(column)} {}

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableCell>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *document) const final {
    if (skip_()) {
      return {};
    }
    std::string type = m_node.attribute("t").value();
    auto child = m_node.child("v");
    if (type == "s") {
      auto replacement =
          shared_strings_(document).at(child.text().as_uint()).first_child();
      return construct_default_element(replacement);
    }
    return construct_default_element(child);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *document) const final {
    TableColumn previous_column;
    if (auto previous_column_ptr =
            m_column.construct_previous_sibling(document)) {
      previous_column = dynamic_cast<const TableColumn &>(*previous_column_ptr);
    }
    if (node_position_(m_node).column() <= previous_column.m_column) {
      return common::construct_2<TableCell>(m_node, m_row, previous_column);
    }
    auto previous_sibling = m_node.previous_sibling("c");
    if (!previous_sibling) {
      return {};
    }
    return common::construct_2<TableCell>(previous_sibling, m_row,
                                          previous_column);
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *document) const final {
    TableColumn next_column;
    if (auto next_column_ptr = m_column.construct_next_sibling(document)) {
      next_column = dynamic_cast<const TableColumn &>(*next_column_ptr);
    }
    if (node_position_(m_node).column() >= next_column.m_column) {
      return common::construct_2<TableCell>(m_node, m_row, next_column);
    }
    auto next_sibling = m_node.next_sibling("c");
    if (!next_sibling) {
      return {};
    }
    return common::construct_2<TableCell>(next_sibling, m_row, next_column);
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
    return {}; // TODO
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] ValueType value_type(const abstract::Document *) const final {
    return ValueType::string;
  }

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    if (skip_()) {
      return {};
    }
    if (auto id = m_node.attribute("s")) {
      return style_registry_(document)->cell_style(id.as_uint());
    }
    return {};
  }

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_cell_style;
  }

private:
  TableRow m_row;
  TableColumn m_column;

  [[nodiscard]] static common::TablePosition
  node_position_(pugi::xml_node node) {
    return common::TablePosition(node.attribute("r").value());
  }

  [[nodiscard]] common::TablePosition position_() const {
    return {m_row.m_row, m_column.m_column};
  }

  [[nodiscard]] bool skip_() const {
    return !m_node || position_() != node_position_(m_node);
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

  [[nodiscard]] std::string content(const abstract::Document *) const final {
    std::string result;
    for (auto node = first_(); is_text_(node); node = node.next_sibling()) {
      result += text_(node);
    }
    return result;
  }

  void set_content(const abstract::Document *, const std::string &) final {
    // TODO
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }

private:
  static bool is_text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "t" || name == "v") {
      return true;
    }

    return false;
  }

  static std::string text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "t" || name == "v") {
      return node.text().get();
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

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<Frame>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_optional<ImageElement>(
        m_node.child("xdr:pic").child("xdr:blipFill").child("a:blip"));
  }

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *) const final {
    return AnchorType::at_page;
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    if (auto x = read_emus_attribute(m_node.child("xdr:pic")
                                         .child("xdr:spPr")
                                         .child("a:xfrm")
                                         .child("a:off")
                                         .attribute("x"))) {
      return x->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    if (auto y = read_emus_attribute(m_node.child("xdr:pic")
                                         .child("xdr:spPr")
                                         .child("a:xfrm")
                                         .child("a:off")
                                         .attribute("y"))) {
      return y->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final {
    if (auto width = read_emus_attribute(m_node.child("xdr:pic")
                                             .child("xdr:spPr")
                                             .child("a:xfrm")
                                             .child("a:ext")
                                             .attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    if (auto height = read_emus_attribute(m_node.child("xdr:pic")
                                              .child("xdr:spPr")
                                              .child("a:xfrm")
                                              .child("a:ext")
                                              .attribute("cy"))) {
      return height->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] GraphicStyle
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    return {};
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
    if (auto ref = m_node.attribute("r:embed")) {
      /* TODO
      auto relations = document_relations_(document);
      if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
        return common::Path("word").join(rel->second).string();
      }*/
    }
    return ""; // TODO
  }
};

} // namespace

std::unique_ptr<abstract::Element>
Element::construct_default_element(pugi::xml_node node) {
  using Constructor =
      std::function<std::unique_ptr<abstract::Element>(pugi::xml_node node)>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"workbook", common::construct<Root>},
      {"worksheet", common::construct<Sheet>},
      {"col", common::construct<TableColumn>},
      {"row", common::construct<TableRow>},
      {"r", common::construct<Span>},
      {"t", common::construct<Text>},
      {"v", common::construct<Text>},
      {"xdr:twoCellAnchor", common::construct<Frame>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node);
  }

  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
