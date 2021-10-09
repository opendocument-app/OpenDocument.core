#include <functional>
#include <internal/abstract/document.h>
#include <internal/common/document_element.h>
#include <internal/common/style.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/presentation/ooxml_presentation_cursor.h>
#include <internal/ooxml/presentation/ooxml_presentation_document.h>
#include <internal/ooxml/presentation/ooxml_presentation_element.h>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/quantity.h>
#include <odr/style.h>
#include <optional>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {

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

pugi::xml_node Element::slide_(const abstract::Document *document,
                               const std::string &id) {
  return document_(document)->m_slides_xml.at(id).document_element();
}

namespace {

class Slide;
class TableColumn;
class TableRow;

template <ElementType element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return element_type;
  }

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const override {
    return common::construct_2<DefaultElement>(allocator, *this);
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<Root>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator *allocator) const final {
    return common::construct_optional<Slide>(
        m_node.child("p:sldIdLst").child("p:sldId"), allocator);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator *) const final {
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *) const final {
    return nullptr;
  }
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<Slide>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *document,
                        const abstract::Allocator *allocator) const final {
    return common::construct_first_child_element(
        construct_default_element,
        slide_node_(document).child("p:cSld").child("p:spTree"), allocator);
  }

  bool move_to_previous_sibling(const abstract::Document *) final {
    if (auto previous_sibling = m_node.previous_sibling("p:sldId")) {
      m_node = previous_sibling;
      return true;
    }
    return false;
  }

  bool move_to_next_sibling(const abstract::Document *) final {
    if (auto next_sibling = m_node.next_sibling("p:sldId")) {
      m_node = next_sibling;
      return true;
    }
    return false;
  }

  [[nodiscard]] PageLayout page_layout(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] Element *master_page(const abstract::Document *,
                                     const abstract::DocumentCursor *,
                                     const abstract::Allocator *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::string name(const abstract::Document *) const final {
    return {}; // TODO
  }

private:
  pugi::xml_node slide_node_(const abstract::Document *document) const {
    return slide_(document, m_node.attribute("r:id").value());
  }
};

class Paragraph final : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
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
  construct_copy(const abstract::Allocator *allocator) const final {
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
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<Text>(allocator, *this);
  }

  abstract::Element *
  construct_previous_sibling(const abstract::Document *,
                             const abstract::Allocator *allocator) const final {
    return common::construct_previous_sibling_element(construct_default_element,
                                                      first_(), allocator);
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *allocator) const final {
    return common::construct_next_sibling_element(construct_default_element,
                                                  last_(), allocator);
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

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<TableElement>(allocator, *this);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator *) const final {
    return nullptr;
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final {
    return {}; // TODO
  }

  abstract::Element *
  construct_first_column(const abstract::Document *,
                         const abstract::Allocator *allocator) const final {
    return common::construct_optional<TableColumn>(
        m_node.child("w:tblGrid").child("w:gridCol"), allocator);
  }

  abstract::Element *
  construct_first_row(const abstract::Document *,
                      const abstract::Allocator *allocator) const final {
    return common::construct_optional<TableRow>(m_node.child("w:tr"),
                                                allocator);
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

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<TableColumn>(allocator, *this);
  }

  abstract::Element *construct_previous_sibling(
      const abstract::Document *,
      const abstract::Allocator *allocator) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:gridCol")) {
      return common::construct_2<TableColumn>(allocator, previous_sibling);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *allocator) const override {
    if (auto next_sibling = m_node.next_sibling("w:gridCol")) {
      return common::construct_2<TableColumn>(allocator, next_sibling);
    }
    return nullptr;
  }

  bool move_to_previous_sibling(const abstract::Document *) final {
    if (auto previous_sibling = m_node.previous_sibling("w:gridCol")) {
      m_node = previous_sibling;
      return true;
    }
    return false;
  }

  bool move_to_next_sibling(const abstract::Document *) final {
    if (auto next_sibling = m_node.next_sibling("w:gridCol")) {
      m_node = next_sibling;
      return true;
    }
    return false;
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
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<TableRow>(allocator, *this);
  }

  abstract::Element *construct_previous_sibling(
      const abstract::Document *,
      const abstract::Allocator *allocator) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:tr")) {
      return common::construct_2<TableColumn>(allocator, previous_sibling);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *allocator) const override {
    if (auto next_sibling = m_node.next_sibling("w:tr")) {
      return common::construct_2<TableColumn>(allocator, next_sibling);
    }
    return nullptr;
  }

  bool move_to_previous_sibling(const abstract::Document *) final {
    if (auto previous_sibling = m_node.previous_sibling("w:tr")) {
      m_node = previous_sibling;
      return true;
    }
    return false;
  }

  bool move_to_next_sibling(const abstract::Document *) final {
    if (auto next_sibling = m_node.next_sibling("w:tr")) {
      m_node = next_sibling;
      return true;
    }
    return false;
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
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<TableCell>(allocator, *this);
  }

  abstract::Element *construct_previous_sibling(
      const abstract::Document *,
      const abstract::Allocator *allocator) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      return common::construct_2<TableColumn>(allocator, previous_sibling);
    }
    return nullptr;
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *allocator) const override {
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      return common::construct_2<TableColumn>(allocator, next_sibling);
    }
    return nullptr;
  }

  bool move_to_previous_sibling(const abstract::Document *) final {
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      m_node = previous_sibling;
      return true;
    }
    return false;
  }

  bool move_to_next_sibling(const abstract::Document *) final {
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      m_node = next_sibling;
      return true;
    }
    return false;
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
    return false;
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

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<Frame>(allocator, *this);
  }

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *) const final {
    return AnchorType::at_page;
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    if (auto x = read_emus_attribute(
            m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
                "x"))) {
      return x->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    if (auto y = read_emus_attribute(
            m_node.child("p:spPr").child("a:xfrm").child("a:off").attribute(
                "y"))) {
      return y->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final {
    if (auto cx = read_emus_attribute(
            m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
                "cx"))) {
      return cx->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    if (auto cy = read_emus_attribute(
            m_node.child("p:spPr").child("a:xfrm").child("a:ext").attribute(
                "cy"))) {
      return cy->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    return {}; // TODO
  }
};

class ImageElement final : public Element, public abstract::ImageElement {
public:
  using Element::Element;

  [[nodiscard]] abstract::Element *
  construct_copy(const abstract::Allocator *allocator) const final {
    return common::construct_2<ImageElement>(allocator, *this);
  }

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

abstract::Element *
Element::construct_default_element(pugi::xml_node node,
                                   const abstract::Allocator *allocator) {
  using Constructor = std::function<abstract::Element *(
      pugi::xml_node node, const abstract::Allocator *allocator)>;

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"p:presentation", common::construct<Root>},
      {"p:sld", common::construct<Slide>},
      {"p:sp", common::construct<Frame>},
      {"p:txBody", common::construct<Group>},
      {"a:t", common::construct<Text>},
      {"a:p", common::construct<Paragraph>},
      {"a:r", common::construct<Span>},
      {"a:tbl", common::construct<TableElement>},
      {"a:gridCol", common::construct<TableColumn>},
      {"a:tr", common::construct<TableRow>},
      {"a:tc", common::construct<TableCell>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node, allocator);
  }

  return {};
}

} // namespace odr::internal::ooxml::presentation
