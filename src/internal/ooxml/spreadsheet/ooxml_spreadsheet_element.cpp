#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
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

namespace {

template <ElementType> class DefaultElement;
class Root;
class Sheet;
class Text;
class TableElement;
class TableColumn;
class TableRow;
class TableCell;

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
        document_(document), m_node.child("p:sldIdLst").child("p:sldId"),
        allocator);
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

class Sheet final : public Element, public abstract::SheetElement {
public:
  using Element::Element;

  abstract::Element *first_child(const abstract::Document *,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *) final {
    return nullptr; // TODO
  }

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("p:sldId")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("p:sldId")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return {}; // TODO
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

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  abstract::Element *first_child(const abstract::Document *,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *) final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final {
    return {}; // TODO
  }

  abstract::Element *
  first_column(const abstract::Document *document,
               const abstract::DocumentCursor *,
               const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableColumn>(
        document_(document), m_node.child("w:tblGrid").child("w:gridCol"),
        allocator);
  }

  abstract::Element *
  first_row(const abstract::Document *document,
            const abstract::DocumentCursor *,
            const abstract::Allocator *allocator) const final {
    return construct_default_optional<TableRow>(
        document_(document), m_node.child("w:tr"), allocator);
  }

  [[nodiscard]] std::optional<TableStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return partial_style(document).table_style;
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  abstract::Element *previous_sibling(const abstract::Document *,
                                      const abstract::DocumentCursor *,
                                      const abstract::Allocator *) final {
    if (auto previous_sibling = m_node.previous_sibling("w:gridCol")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("w:gridCol")) {
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
    if (auto previous_sibling = m_node.previous_sibling("w:tr")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("w:tr")) {
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
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *) final {
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      m_node = next_sibling;
      return this;
    }
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

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

  [[nodiscard]] std::optional<std::string>
  anchor_type(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    return read_optional_attribute(
        m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute("x"),
        read_emus_attribute);
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    return read_optional_attribute(
        m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute("y"),
        read_emus_attribute);
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final {
    return read_optional_attribute(
        m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute("cx"),
        read_emus_attribute);
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    return read_optional_attribute(
        m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute("cy"),
        read_emus_attribute);
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final {
    return {}; // TODO
  }
};

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] bool internal(const abstract::Document *) const final {
    return false;
  }

  [[nodiscard]] std::optional<odr::File>
  file(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::string href(const abstract::Document *) const final {
    return ""; // TODO
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

  using Span = DefaultElement<ElementType::span>;
  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"p:presentation", construct_default<Root>},
      {"p:sld", construct_default<Sheet>},
      {"p:sp", construct_default<Frame>},
      {"p:txBody", construct_default<Group>},
      {"a:t", construct_default<Text>},
      {"a:p", construct_default<Paragraph>},
      {"a:r", construct_default<Span>},
      {"a:tbl", construct_default<TableElement>},
      {"a:gridCol", construct_default<TableColumn>},
      {"a:tr", construct_default<TableRow>},
      {"a:tc", construct_default<TableCell>},
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
