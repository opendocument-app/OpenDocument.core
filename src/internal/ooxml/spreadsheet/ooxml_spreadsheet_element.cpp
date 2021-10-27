#include <functional>
#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <internal/common/table_range.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_element.h>
#include <odr/style.h>
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const override {
    return common::construct_2<DefaultElement>(allocator, *this);
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Root>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator &allocator) const final {
    return common::construct_optional<Sheet>(
        m_node.child("sheets").child("sheet"), allocator);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &) const final {
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &) const final {
    return nullptr;
  }
};

class Sheet final : public Element,
                    public abstract::SheetElement,
                    public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Sheet>(allocator, *this);
  }

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::sheet;
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator &) const final {
    return nullptr;
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    return common::construct_optional<Sheet>(m_node.previous_sibling("sheet"),
                                             allocator);
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    return common::construct_optional<Sheet>(m_node.next_sibling("sheet"),
                                             allocator);
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

  [[nodiscard]] abstract::Element *
  construct_first_column(const abstract::Document *document,
                         const abstract::Allocator &allocator) const final {
    return common::construct_optional<TableColumn>(
        sheet_node_(document).child("cols").child("col"), allocator);
  }

  [[nodiscard]] abstract::Element *
  construct_first_row(const abstract::Document *document,
                      const abstract::Allocator &allocator) const final {
    return common::construct_optional<TableRow>(
        sheet_node_(document).child("sheetData").child("row"), allocator);
  }

  [[nodiscard]] abstract::Element *
  construct_first_shape(const abstract::Document *document,
                        const abstract::Allocator &allocator) const final {
    return common::construct_first_child_element(
        construct_default_element, drawing_node_(document), allocator);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableColumn>(allocator, *this);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    if (min_() <= m_column - 1) {
      return common::construct_2<TableColumn>(allocator, m_node, m_column - 1);
    }
    if (auto previous_sibling = m_node.previous_sibling()) {
      return common::construct_2<TableColumn>(allocator, previous_sibling,
                                              m_column - 1);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (max_() >= m_column + 1) {
      return common::construct_2<TableColumn>(allocator, m_node, m_column + 1);
    }
    if (auto next_sibling = m_node.next_sibling()) {
      return common::construct_2<TableColumn>(allocator, next_sibling,
                                              m_column + 1);
    }
    return nullptr;
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableRow>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator &allocator) const final {
    return common::construct_2<TableCell>(
        allocator, m_node.child("c"), *this,
        TableColumn(m_node.parent().parent().child("cols").child("col")));
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    if (auto previous_sibling = m_node.previous_sibling()) {
      return common::construct_2<TableRow>(allocator, previous_sibling,
                                           m_row - 1);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (auto next_sibling = m_node.next_sibling()) {
      return common::construct_2<TableRow>(allocator, next_sibling, m_row + 1);
    }
    return nullptr;
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

  friend class TableCell;
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  // TODO remove
  using Element::Element;

  TableCell(pugi::xml_node node, TableRow row, TableColumn column)
      : Element(node), m_row{std::move(row)}, m_column{std::move(column)} {}

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableCell>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *document,
                        const abstract::Allocator &allocator) const final {
    if (skip_()) {
      return nullptr;
    }
    std::string type = m_node.attribute("t").value();
    auto child = m_node.child("v");
    if (type == "s") {
      auto replacement =
          shared_strings_(document).at(child.text().as_uint()).first_child();
      return construct_default_element(replacement, allocator);
    }
    return construct_default_element(child, allocator);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *document,
                             const abstract::Allocator &allocator) const final {
    auto previous_sibling = m_node.previous_sibling("c");
    if (!previous_sibling) {
      return nullptr;
    }
    TableColumn previous_column;
    m_column.construct_previous_sibling(
        document, [&](std::size_t) { return &previous_column; });
    if (node_position_(previous_sibling).column() != previous_column.m_column) {
      previous_sibling = m_node;
    }
    return common::construct_2<TableCell>(allocator, previous_sibling, m_row,
                                          previous_column);
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *document,
                         const abstract::Allocator &allocator) const final {
    auto next_sibling = m_node.next_sibling("c");
    if (!next_sibling) {
      return nullptr;
    }
    TableColumn next_column;
    m_column.construct_next_sibling(document,
                                    [&](std::size_t) { return &next_column; });
    if (node_position_(next_sibling).column() != next_column.m_column) {
      next_sibling = m_node;
    }
    return common::construct_2<TableCell>(allocator, next_sibling, m_row,
                                          next_column);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Span>(allocator, *this);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Text>(allocator, *this);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Frame>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator &allocator) const final {
    return common::construct_optional<ImageElement>(
        m_node.child("xdr:pic").child("xdr:blipFill").child("a:blip"),
        allocator);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<ImageElement>(allocator, *this);
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

abstract::Element *
Element::construct_default_element(pugi::xml_node node,
                                   const abstract::Allocator &allocator) {
  using Constructor = std::function<abstract::Element *(
      pugi::xml_node node, const abstract::Allocator &allocator)>;

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
    return constructor_it->second(node, allocator);
  }

  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
