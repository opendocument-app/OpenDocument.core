#include <cstdint>
#include <functional>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/document_element.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_cursor.hpp>
#include <odr/internal/ooxml/text/ooxml_text_document.hpp>
#include <odr/internal/ooxml/text/ooxml_text_element.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>
#include <optional>
#include <pugixml.hpp>
#include <utility>

namespace odr::internal::ooxml::text {

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

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &dynamic_cast<const Document *>(document)->m_style_registry;
}

const std::unordered_map<std::string, std::string> &
Element::document_relations_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document)->m_document_relations;
}

namespace {

class ListItemParagraph;
class TableColumn;
class TableRow;

template <ElementType _element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
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
  construct_previous_sibling(const abstract::Document *) const final {
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return {};
  }
};

class Paragraph : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const override {
    return common::construct_2<Paragraph>(*this);
  }

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    return style_(document)->partial_paragraph_style(m_node);
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

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    return style_(document)->partial_text_style(m_node);
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
    // TODO http://officeopenxml.com/WPtextSpacing.php
    // <w:t xml:space="preserve">
    // use `xml:space`
    auto parent = m_node.parent();
    auto old_start = m_node;

    auto tokens = util::xml::tokenize_text(text);
    for (auto &&token : tokens) {
      switch (token.type) {
      case util::xml::StringToken::Type::none:
        break;
      case util::xml::StringToken::Type::string: {
        auto text_node = parent.insert_child_before("w:t", old_start);
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::spaces: {
        auto text_node = parent.insert_child_before("w:t", old_start);
        text_node.append_attribute("xml:space").set_value("preserve");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          parent.insert_child_before("w:tab", old_start);
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
    std::string name = node.name();

    if (name == "w:t") {
      return true;
    }
    if (name == "w:tab") {
      return true;
    }

    return false;
  }

  static std::string text_(const pugi::xml_node node) {
    std::string name = node.name();

    if (name == "w:t") {
      return node.text().get();
    }
    if (name == "w:tab") {
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

  [[nodiscard]] std::string
  href(const abstract::Document *document) const final {
    if (auto anchor = m_node.attribute("w:anchor")) {
      return std::string("#") + anchor.value();
    }
    if (auto ref = m_node.attribute("r:id")) {
      auto relations = document_relations_(document);
      if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
        return rel->second;
      }
    }
    return "";
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
    return m_node.attribute("w:name").value();
  }
};

class ListElement final : public Element, public abstract::ListItemElement {
public:
  static bool is_list_item(const pugi::xml_node node) {
    return node.child("w:pPr").child("w:numPr");
  }

  static std::int32_t level(const pugi::xml_node node) {
    return node.child("w:pPr")
        .child("w:numPr")
        .child("w:ilvl")
        .attribute("w:val")
        .as_int(0);
  }

  ListElement(pugi::xml_node node, const std::int32_t level)
      : Element(node), m_level{level} {}

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    if (m_level == level(m_node)) {
      return ElementType::list_item;
    }
    return ElementType::list;
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<ListElement>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    auto node_level = level(m_node);
    if (m_level == node_level) {
      return common::construct<ListItemParagraph>(m_node);
    } else if (m_level < node_level) {
      return common::construct_2<ListElement>(m_node, m_level + 1);
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return {}; // TODO
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    if (auto node = m_node.next_sibling(); node && is_list_item(node)) {
      return common::construct_2<ListElement>(node, m_level);
    }
    return {};
  }

  [[nodiscard]] TextStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }

private:
  std::int32_t m_level{0};
};

class ListRoot final : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const final {
    return ElementType::list;
  }

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<ListRoot>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_first_child(const abstract::Document *) const final {
    return common::construct_2<ListElement>(m_node, 0);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    auto node = m_node.previous_sibling();
    for (; node && ListElement::is_list_item(node);
         node = node.previous_sibling()) {
    }
    return construct_default_element(node);
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    auto node = m_node.next_sibling();
    for (; node && ListElement::is_list_item(node);
         node = node.next_sibling()) {
    }
    return construct_default_element(node);
  }
};

class ListItemParagraph : public Paragraph {
public:
  using Paragraph::Paragraph;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<ListItemParagraph>(*this);
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

class TableElement : public Element, public abstract::TableElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableElement>(*this);
  }

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

  [[nodiscard]] TableStyle style(const abstract::Document *document,
                                 const abstract::DocumentCursor *) const final {
    return style_(document)->partial_table_style(m_node).table_style;
  }
};

class TableColumn final : public Element, public abstract::TableColumnElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableColumn>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<TableColumn>(
        m_node.previous_sibling("w:gridCol"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<TableColumn>(
        m_node.next_sibling("w:gridCol"));
  }

  [[nodiscard]] TableColumnStyle
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    TableColumnStyle result;
    if (auto width = read_twips_attribute(m_node.attribute("w:w"))) {
      result.width = width;
    }
    return result;
  }
};

class TableRow final : public Element, public abstract::TableRowElement {
public:
  using Element::Element;

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableRow>(*this);
  }

  std::unique_ptr<abstract::Element>
  construct_previous_sibling(const abstract::Document *) const final {
    return common::construct_optional<TableRow>(
        m_node.previous_sibling("w:tr"));
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *) const final {
    return common::construct_optional<TableRow>(m_node.next_sibling("w:tr"));
  }

  [[nodiscard]] TableRowStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return style_(document)->partial_table_row_style(m_node).table_row_style;
  }
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  // TODO this only works for first cells
  explicit TableCell(pugi::xml_node node)
      : Element(node),
        m_column{node.parent().parent().child("w:tblGrid").child("w:gridCol")},
        m_row{node.parent()} {}
  TableCell(pugi::xml_node node, TableColumn column)
      : Element(node), m_column{std::move(column)}, m_row{node.parent()} {}

  [[nodiscard]] std::unique_ptr<abstract::Element>
  construct_copy() const final {
    return common::construct_2<TableCell>(*this);
  }

  std::unique_ptr<abstract::Element> construct_previous_sibling(
      const abstract::Document *document) const override {
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      auto previous_column = m_column.construct_previous_sibling(document);
      return common::construct_2<TableCell>(
          previous_sibling,
          dynamic_cast<const TableColumn &>(*previous_column.get()));
    }
    return {};
  }

  std::unique_ptr<abstract::Element>
  construct_next_sibling(const abstract::Document *document) const override {
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      auto next_column = m_column.construct_next_sibling(document);
      return common::construct_2<TableCell>(
          next_sibling, dynamic_cast<const TableColumn &>(*next_column.get()));
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
    return false;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] ValueType value_type(const abstract::Document *) const final {
    return ValueType::string;
  }

  [[nodiscard]] TableCellStyle
  style(const abstract::Document *document,
        const abstract::DocumentCursor *) const final {
    return style_(document)->partial_table_cell_style(m_node).table_cell_style;
  }

private:
  TableColumn m_column;
  TableRow m_row;
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
    return common::construct_first_child_element(
        construct_default_element, inner_node_().child("a:graphic"));
  }

  [[nodiscard]] AnchorType anchor_type(const abstract::Document *) const final {
    if (m_node.child("wp:inline")) {
      return AnchorType::as_char;
    }
    return AnchorType::as_char; // TODO default?
  }

  [[nodiscard]] std::optional<std::string>
  x(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  y(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  width(const abstract::Document *) const final {
    if (auto width = read_emus_attribute(
            inner_node_().child("wp:extent").attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    if (auto height = read_emus_attribute(
            inner_node_().child("wp:extent").attribute("cy"))) {
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

private:
  [[nodiscard]] pugi::xml_node inner_node_() const {
    if (auto anchor = m_node.child("wp:anchor")) {
      return anchor;
    } else if (auto inline_node = m_node.child("wp:inline")) {
      return inline_node;
    }
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

  [[nodiscard]] std::string
  href(const abstract::Document *document) const final {
    if (auto ref = m_node.child("pic:pic")
                       .child("pic:blipFill")
                       .child("a:blip")
                       .attribute("r:embed")) {
      auto relations = document_relations_(document);
      if (auto rel = relations.find(ref.value()); rel != std::end(relations)) {
        return common::Path("word").join(rel->second).string();
      }
    }
    return ""; // TODO
  }
};

} // namespace

std::unique_ptr<abstract::Element>
Element::construct_default_element(pugi::xml_node node) {
  using Constructor =
      std::function<std::unique_ptr<abstract::Element>(pugi::xml_node node)>;

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"w:body", common::construct<Root>},
      {"w:t", common::construct<Text>},
      {"w:tab", common::construct<Text>},
      {"w:p", common::construct<Paragraph>},
      {"w:r", common::construct<Span>},
      {"w:bookmarkStart", common::construct<Bookmark>},
      {"w:hyperlink", common::construct<Link>},
      {"w:tbl", common::construct<TableElement>},
      {"w:gridCol", common::construct<TableColumn>},
      {"w:tr", common::construct<TableRow>},
      {"w:tc", common::construct<TableCell>},
      {"w:sdt", common::construct<Group>},
      {"w:sdtContent", common::construct<Group>},
      {"w:drawing", common::construct<Frame>},
      {"wp:anchor", common::construct<Group>},
      {"a:graphicData", common::construct<ImageElement>},
  };

  if (ListElement::is_list_item(node)) {
    return common::construct<ListRoot>(node);
  }

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(node);
  }

  return {};
}

} // namespace odr::internal::ooxml::text
