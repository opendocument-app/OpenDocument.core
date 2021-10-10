#include <functional>
#include <internal/abstract/document.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <internal/common/table_range.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_element.h>
#include <odr/document.h>
#include <odr/style.h>
#include <optional>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::spreadsheet {

Element::Element(pugi::xml_node node) : common::Element<Element>(node) {}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {}; // TODO
}

common::ResolvedStyle
Element::intermediate_style(const abstract::Document *,
                            const abstract::DocumentCursor *cursor) const {
  return static_cast<const DocumentCursor *>(cursor)->intermediate_style();
}

const Document *Element::document_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document);
}

pugi::xml_node Element::sheet_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_sheets_xml.at(id).document_element();
}

namespace {

class Sheet;
class TableColumn;
class TableRow;

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
    if (auto previous_sibling = m_node.previous_sibling("sheet")) {
      return common::construct<Sheet>(previous_sibling, allocator);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (auto next_sibling = m_node.next_sibling("sheet")) {
      return common::construct<Sheet>(next_sibling, allocator);
    }
    return nullptr;
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
        return {range.to().row(), range.to().column()};
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
  construct_first_shape(const abstract::Document *,
                        const abstract::Allocator &) const final {
    return nullptr;
  }

  [[nodiscard]] std::optional<TableStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_style;
  }

private:
  pugi::xml_node sheet_node_(const abstract::Document *document) const {
    return sheet_(document, m_node.attribute("r:id").value());
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableColumn>(allocator, *this);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    if (auto previous_sibling = m_node.previous_sibling("col")) {
      return common::construct<TableColumn>(previous_sibling, allocator);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (auto next_sibling = m_node.next_sibling("col")) {
      return common::construct<TableColumn>(next_sibling, allocator);
    }
    return nullptr;
  }

  [[nodiscard]] std::optional<TableColumnStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_column_style;
  }
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableRow>(allocator, *this);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    if (auto previous_sibling = m_node.previous_sibling("row")) {
      return common::construct<TableColumn>(previous_sibling, allocator);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (auto next_sibling = m_node.next_sibling("row")) {
      return common::construct<TableColumn>(next_sibling, allocator);
    }
    return nullptr;
  }

  [[nodiscard]] std::optional<TableRowStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_row_style;
  }
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<TableCell>(allocator, *this);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator &allocator) const final {
    if (auto previous_sibling = m_node.previous_sibling("c")) {
      return common::construct<TableColumn>(previous_sibling, allocator);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator &allocator) const final {
    if (auto next_sibling = m_node.next_sibling("c")) {
      return common::construct<TableColumn>(next_sibling, allocator);
    }
    return nullptr;
  }

  [[nodiscard]] abstract::Element *
  column(const abstract::Document *, const abstract::DocumentCursor *) final {
    return nullptr;
  }

  [[nodiscard]] abstract::Element *row(const abstract::Document *,
                                       const abstract::DocumentCursor *) final {
    return nullptr;
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

  [[nodiscard]] std::optional<TableCellStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_cell_style;
  }
};

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Paragraph>(allocator, *this);
  }

  [[nodiscard]] std::optional<ParagraphStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).paragraph_style;
  }

  [[nodiscard]] std::optional<TextStyle>
  text_style(const abstract::Document *document,
             const abstract::DocumentCursor *) const final {
    return partial_style(document).text_style;
  }
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator &allocator) const final {
    return common::construct_2<Span>(allocator, *this);
  }

  [[nodiscard]] std::optional<TextStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).text_style;
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

  [[nodiscard]] std::optional<TextStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).text_style;
  }

private:
  static bool is_text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "a:t") {
      return true;
    }
    if (name == "a:tab") {
      return true;
    }

    return false;
  }

  static std::string text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "a:t") {
      return node.text().get();
    }
    if (name == "a:tab") {
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
      {"c", common::construct<TableCell>},
      {"t", common::construct<Text>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node, allocator);
  }

  return {};
}

} // namespace odr::internal::ooxml::spreadsheet
