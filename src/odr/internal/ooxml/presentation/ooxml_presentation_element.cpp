#include <functional>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_element.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {

namespace {
std::tuple<abstract::Element *, pugi::xml_node>
parse_element_tree(pugi::xml_node node,
                   std::vector<std::unique_ptr<abstract::Element>> &store);

template <typename Derived>
std::tuple<abstract::Element *, pugi::xml_node> default_parse_element_tree(
    pugi::xml_node node,
    std::vector<std::unique_ptr<abstract::Element>> &store);
} // namespace

Element::Element(pugi::xml_node node) : common::Element(node) {}

common::ResolvedStyle Element::partial_style(const abstract::Document *) const {
  return {}; // TODO
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
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

  abstract::Element *first_child(const abstract::Document *) const final {
    return common::construct_optional<Slide>(
        m_node.child("p:sldIdLst").child("p:sldId"));
  }
};

class Slide final : public Element, public abstract::SlideElement {
public:
  using Element::Element;

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *document) const final {
    return common::construct_first_child_element(
        construct_default_element,
        slide_node_(document).child("p:cSld").child("p:spTree"));
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<Slide>(
        m_node.previous_sibling("p:sldId"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<Slide>(m_node.next_sibling("p:sldId"));
  }

  [[nodiscard]] PageLayout page_layout(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_master_page(const abstract::Document *) const final {
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

  [[nodiscard]] ParagraphStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).paragraph_style;
  }

  [[nodiscard]] TextStyle
  text_style(const abstract::Document *document) const final {
    return partial_style(document).text_style;
  }
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  [[nodiscard]] TextStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).text_style;
  }
};

class Text final : public Element, public abstract::TextElement {
public:
  using Element::Element;

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

  void set_content(const abstract::Document *, const std::string &) final {
    // TODO
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document) const final {
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

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] TableDimensions
  dimensions(const abstract::Document *) const final {
    return {}; // TODO
  }

  std::unique_ptr<abstract::Element>
  construct_first_column(const abstract::Document *) const final {
    return common::construct_optional<TableColumn>(
        m_node.child("w:tblGrid").child("w:gridCol"));
  }

  std::unique_ptr<abstract::Element>
  construct_first_row(const abstract::Document *) const final {
    return common::construct_optional<TableRow>(m_node.child("w:tr"));
  }

  [[nodiscard]] TableStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_style;
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:gridCol")) {
      return common::construct_2<TableColumn>(previous_sibling);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const override {
    if (auto next_sibling = m_node.next_sibling("w:gridCol")) {
      return common::construct_2<TableColumn>(next_sibling);
    }
    return {};
  }

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_column_style;
  }
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:tr")) {
      return common::construct_2<TableColumn>(previous_sibling);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const override {
    if (auto next_sibling = m_node.next_sibling("w:tr")) {
      return common::construct_2<TableColumn>(next_sibling);
    }
    return {};
  }

  [[nodiscard]] TableRowStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_row_style;
  }
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  using Element::Element;

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      return common::construct_2<TableColumn>(previous_sibling);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const override {
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      return common::construct_2<TableColumn>(next_sibling);
    }
    return {};
  }

  [[nodiscard]] abstract::Element *
  column(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] abstract::Element *row(const abstract::Document *) const final {
    return {};
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

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document) const final {
    return partial_style(document).table_cell_style;
  }
};

class Frame final : public Element, public abstract::FrameElement {
public:
  using Element::Element;

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

  [[nodiscard]] GraphicStyle style(const abstract::Document *) const final {
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

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Parser> parser_table{
      {"p:presentation", default_parse_element_tree<Root>},
      {"p:sld", default_parse_element_tree<Slide>},
      {"p:sp", default_parse_element_tree<Frame>},
      {"p:txBody", default_parse_element_tree<Group>},
      {"a:t", default_parse_element_tree<Text>},
      {"a:p", default_parse_element_tree<Paragraph>},
      {"a:r", default_parse_element_tree<Span>},
      {"a:tbl", default_parse_element_tree<TableElement>},
      {"a:gridCol", default_parse_element_tree<TableColumn>},
      {"a:tr", default_parse_element_tree<TableRow>},
      {"a:tc", default_parse_element_tree<TableCell>},
  };

  if (auto constructor_it = parser_table.find(node.name());
      constructor_it != std::end(parser_table)) {
    return constructor_it->second(node, store);
  }

  return std::make_tuple(nullptr, pugi::xml_node());
}

} // namespace

} // namespace odr::internal::ooxml::presentation

namespace odr::internal::ooxml {

std::tuple<abstract::Element *, std::vector<std::unique_ptr<abstract::Element>>>
presentation::parse_tree(pugi::xml_node node) {
  std::vector<std::unique_ptr<abstract::Element>> store;
  auto [root, _] = parse_element_tree(node, store);
  return std::make_tuple(root, store);
}

} // namespace odr::internal::ooxml
