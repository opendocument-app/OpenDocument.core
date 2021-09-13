#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_range.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_element.h>
#include <odr/file.h>

namespace odr::internal::ooxml::spreadsheet {

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

pugi::xml_node Element::root_(const abstract::Document *document) {
  return document_(document)->m_workbook_xml.document_element();
}

pugi::xml_node Element::sheet_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_sheets_xml.at(id).document_element();
}

namespace {

template <ElementType> class DefaultElement;
class Root;
class Sheet;
class TableColumn;
class TableRow;
class TableCell;
class Text;

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

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return element_type;
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_optional<Sheet>(
        document_(document), m_node.child("sheets").child("sheet"), allocator);
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

class Sheet final : public Element,
                    public abstract::SheetElement,
                    public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::sheet;
  }

  abstract::Element *first_child(const abstract::Document *,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *) final {
    return nullptr;
  }

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("sheet")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("sheet")) {
      m_node = next_sibling;
      return this;
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

  [[nodiscard]] TableDimensions content(const abstract::Document *document,
                                        std::optional<TableDimensions>) const {
    return dimensions(document); // TODO
  }

  abstract::Element *
  first_column(const abstract::Document *document,
               const abstract::DocumentCursor *,
               const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableColumn>(
        document_(document), sheet_node_(document).child("cols").child("col"),
        allocator);
  }

  abstract::Element *
  first_row(const abstract::Document *document,
            const abstract::DocumentCursor *,
            const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableRow>(
        document_(document),
        sheet_node_(document).child("sheetData").child("row"), allocator);
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

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("col")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("col")) {
      m_node = next_sibling;
      return this;
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

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("row")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("row")) {
      m_node = next_sibling;
      return this;
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

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("c")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("c")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] const abstract::Element *
  column(const abstract::Document *,
         const abstract::DocumentCursor *) const final {
    return nullptr;
  }

  [[nodiscard]] const abstract::Element *
  row(const abstract::Document *,
      const abstract::DocumentCursor *) const final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {}; // TODO
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

  [[nodiscard]] std::optional<ParagraphStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).paragraph_style;
  }
};

class Text final : public Element, public abstract::TextElement {
public:
  using Element::Element;

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

} // namespace odr::internal::ooxml::spreadsheet

namespace odr::internal::ooxml {

abstract::Element *
spreadsheet::construct_default_element(const Document *document,
                                       pugi::xml_node node,
                                       const abstract::Allocator *allocator) {
  using Constructor = std::function<abstract::Element *(
      const Document *document, pugi::xml_node node,
      const abstract::Allocator *allocator)>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"workbook", construct_default<Root>},
      {"worksheet", construct_default<Sheet>},
      {"col", construct_default<TableColumn>},
      {"row", construct_default<TableRow>},
      {"c", construct_default<TableCell>},
      {"t", construct_default<Text>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node, allocator);
  }

  return {};
}

abstract::Element *spreadsheet::construct_default_parent_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *spreadsheet::construct_default_first_child_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *spreadsheet::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *spreadsheet::construct_default_next_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

} // namespace odr::internal::ooxml
