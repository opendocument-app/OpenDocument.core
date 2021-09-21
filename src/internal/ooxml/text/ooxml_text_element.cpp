#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_cursor.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_cursor.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/ooxml/text/ooxml_text_element.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <odr/file.h>

namespace odr::internal::ooxml::text {

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

const StyleRegistry *Element::style_(const abstract::Document *document) {
  return &dynamic_cast<const Document *>(document)->m_style_registry;
}

const std::unordered_map<std::string, std::string> &
Element::document_relations_(const abstract::Document *document) {
  return dynamic_cast<const Document *>(document)->m_document_relations;
}

namespace {

template <ElementType> class DefaultElement;
class Root;
class Text;
class ListItem;
class ListItemParagraph;
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

template <typename Derived, typename... Args>
abstract::Element *construct_default_2(const abstract::Allocator *allocator,
                                       Args &&...args) {
  auto alloc = (*allocator)(sizeof(Derived));
  return new (alloc) Derived(std::forward<Args>(args)...);
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

template <ElementType _element_type> class DefaultElement : public Element {
public:
  using Element::Element;

  [[nodiscard]] ElementType type(const abstract::Document *) const override {
    return _element_type;
  }
};

class Root final : public DefaultElement<ElementType::root> {
public:
  using DefaultElement::DefaultElement;

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

class Paragraph : public Element, public abstract::ParagraphElement {
public:
  using Element::Element;

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    return style_(document)->partial_paragraph_style(m_node);
  }

  [[nodiscard]] std::optional<ParagraphStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).paragraph_style;
  }
};

class Span final : public Element, public abstract::SpanElement {
public:
  using Element::Element;

  common::ResolvedStyle
  partial_style(const abstract::Document *document) const final {
    return style_(document)->partial_text_style(m_node);
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

class List final : public DefaultElement<ElementType::list> {
public:
  static bool is_list_item(const pugi::xml_node node) {
    return false; // TODO
  }

  List(const Document *document, pugi::xml_node node,
       const std::int32_t level = 0)
      : DefaultElement(document, node), m_level{level} {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_2<ListItem>(allocator, document_(document), m_node,
                                         m_level);
  }

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

private:
  std::int32_t m_level{0};

  [[nodiscard]] pugi::xml_node first_() const {
    auto node = m_node;
    for (; is_list_item(node.previous_sibling());
         node = node.previous_sibling()) {
    }
    return node;
  }

  [[nodiscard]] pugi::xml_node last_() const {
    auto node = m_node;
    for (; is_list_item(node.next_sibling()); node = node.next_sibling()) {
    }
    return node;
  }
};

class ListItem final : public Element, public abstract::ListItemElement {
public:
  static std::int32_t level(const pugi::xml_node node) { return 0; }

  ListItem(const Document *document, pugi::xml_node node,
           const std::int32_t level = 0)
      : Element(document, node), m_level{level} {}

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    if (m_level == level(m_node)) {
      return construct_default<ListItemParagraph>(document_(document), m_node,
                                                  allocator);
    }
    return construct_default_2<List>(allocator, document_(document), m_node,
                                     m_level + 1);
  }

  abstract::Element *
  previous_sibling(const abstract::Document *document,
                   const abstract::DocumentCursor *,
                   const abstract::Allocator *allocator) final {
    return nullptr; // TODO
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const abstract::DocumentCursor *,
                                  const abstract::Allocator *allocator) final {
    return nullptr; // TODO
  }

  [[nodiscard]] std::optional<TextStyle>
  style(const abstract::Document *document,
        const abstract::DocumentCursor *cursor) const final {
    return intermediate_style(document, cursor).text_style;
  }

private:
  std::int32_t m_level{0};
};

class ListItemParagraph : public Paragraph {
public:
  using Paragraph::Paragraph;

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
    return style_(document)->partial_table_style(m_node).table_style;
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
    return style_(document)->partial_table_row_style(m_node).table_row_style;
  }
};

class TableCell final : public Element, public abstract::TableCellElement {
public:
  // TODO this only works for first cells
  TableCell(const Document *document, pugi::xml_node node)
      : Element(document, node),
        m_column{document,
                 node.parent().parent().child("w:tblGrid").child("w:gridCol")},
        m_row{document, node.parent()} {}

  abstract::Element *previous_sibling(const abstract::Document *document,
                                      const abstract::DocumentCursor *cursor,
                                      const abstract::Allocator *) final {
    m_column.previous_sibling(document, cursor, nullptr);
    if (auto previous_sibling = m_node.previous_sibling("w:tc")) {
      m_node = previous_sibling;
      return this;
    }
    return nullptr;
  }

  abstract::Element *next_sibling(const abstract::Document *document,
                                  const abstract::DocumentCursor *cursor,
                                  const abstract::Allocator *) final {
    m_column.next_sibling(document, cursor, nullptr);
    if (auto next_sibling = m_node.next_sibling("w:tc")) {
      m_node = next_sibling;
      return this;
    }
    return nullptr;
  }

  [[nodiscard]] const abstract::Element *
  column(const abstract::Document *,
         const abstract::DocumentCursor *) const final {
    return &m_column;
  }

  [[nodiscard]] const abstract::Element *
  row(const abstract::Document *,
      const abstract::DocumentCursor *) const final {
    return &m_row;
  }

  [[nodiscard]] bool covered(const abstract::Document *) const final {
    return false;
  }

  [[nodiscard]] TableDimensions span(const abstract::Document *) const final {
    return {}; // TODO
  }

  [[nodiscard]] std::optional<TableCellStyle>
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

  abstract::Element *first_child(const abstract::Document *document,
                                 const abstract::DocumentCursor *,
                                 const abstract::Allocator *allocator) final {
    return construct_default_first_child_element(
        document_(document), m_node.child("wp:inline").child("a:graphic"),
        allocator);
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
            m_node.child("wp:inline").child("wp:extent").attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  height(const abstract::Document *) const final {
    if (auto height = read_emus_attribute(
            m_node.child("wp:inline").child("wp:extent").attribute("cy"))) {
      return height->to_string();
    }
    return {};
  }

  [[nodiscard]] std::optional<std::string>
  z_index(const abstract::Document *) const final {
    return {};
  }

  [[nodiscard]] std::optional<GraphicStyle>
  style(const abstract::Document *,
        const abstract::DocumentCursor *) const final {
    return {};
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

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

abstract::Element *
text::construct_default_element(const Document *document, pugi::xml_node node,
                                const abstract::Allocator *allocator) {
  using Constructor = std::function<abstract::Element *(
      const Document *document, pugi::xml_node node,
      const abstract::Allocator *allocator)>;

  using Group = DefaultElement<ElementType::group>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"w:body", construct_default<Root>},
      {"w:t", construct_default<Text>},
      {"w:tab", construct_default<Text>},
      {"w:p", construct_default<Paragraph>},
      {"w:r", construct_default<Span>},
      {"w:bookmarkStart", construct_default<Bookmark>},
      {"w:hyperlink", construct_default<Link>},
      {"w:tbl", construct_default<TableElement>},
      {"w:gridCol", construct_default<TableColumn>},
      {"w:tr", construct_default<TableRow>},
      {"w:tc", construct_default<TableCell>},
      {"w:sdt", construct_default<Group>},
      {"w:sdtContent", construct_default<Group>},
      {"w:drawing", construct_default<Frame>},
      {"a:graphicData", construct_default<ImageElement>},
  };

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node, allocator);
  }

  return {};
}

abstract::Element *
text::construct_default_parent_element(const Document *document,
                                       pugi::xml_node node,
                                       const abstract::Allocator *allocator) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_first_child_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_previous_sibling_element(
    const Document *document, pugi::xml_node node,
    const abstract::Allocator *allocator) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = construct_default_element(document, node, allocator)) {
      return result;
    }
  }
  return {};
}

abstract::Element *text::construct_default_next_sibling_element(
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
