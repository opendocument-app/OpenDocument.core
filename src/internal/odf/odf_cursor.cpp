#include <internal/odf/odf_cursor.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
DocumentCursor::Element *
construct_default_element(const OpenDocument *document, pugi::xml_node node,
                          DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_parent_element(const OpenDocument *document,
                                 pugi::xml_node node,
                                 DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_first_child_element(const OpenDocument *document,
                                      pugi::xml_node node,
                                      DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_previous_sibling_element(const OpenDocument *document,
                                           pugi::xml_node node,
                                           DocumentCursor::Allocator allocator);
DocumentCursor::Element *
construct_default_next_sibling_element(const OpenDocument *document,
                                       pugi::xml_node node,
                                       DocumentCursor::Allocator allocator);
} // namespace

template <typename Derived, ElementType _element_type>
class DocumentCursor::DefaultElement : public DocumentCursor::Element {
public:
  pugi::xml_node m_node;

  explicit DefaultElement(pugi::xml_node node) : m_node{node} {}

  bool operator==(const Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  bool operator!=(const Element &rhs) const override {
    return m_node != *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  ElementType type() const final { return _element_type; }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &) const override {
    return {};
  }

  abstract::TableElement *table() const final { return nullptr; }

  abstract::ImageElement *image() const final { return nullptr; }

  Element *first_child(const common::DocumentCursor &cursor,
                       DocumentCursor::Allocator allocator) const override {
    return construct_default_first_child_element(
        dynamic_cast<const odf::DocumentCursor &>(cursor).m_document, m_node,
        allocator);
  }

  Element *
  previous_sibling(const common::DocumentCursor &cursor,
                   DocumentCursor::Allocator allocator) const override {
    return construct_default_previous_sibling_element(
        dynamic_cast<const odf::DocumentCursor &>(cursor).m_document, m_node,
        allocator);
  }

  Element *next_sibling(const common::DocumentCursor &cursor,
                        DocumentCursor::Allocator allocator) const override {
    return construct_default_next_sibling_element(
        dynamic_cast<const odf::DocumentCursor &>(cursor).m_document, m_node,
        allocator);
  }
};

template <typename Derived>
class DefaultRoot
    : public DocumentCursor::DefaultElement<Derived, ElementType::ROOT> {
public:
  explicit DefaultRoot(pugi::xml_node node)
      : DocumentCursor::DefaultElement<Derived, ElementType::ROOT>(node) {}

  DocumentCursor::Element *
  previous_sibling(const common::DocumentCursor &,
                   DocumentCursor::Allocator) const final {
    return nullptr;
  }

  DocumentCursor::Element *next_sibling(const common::DocumentCursor &,
                                        DocumentCursor::Allocator) const final {
    return nullptr;
  }
};

class TextDocumentRoot final : public DefaultRoot<TextDocumentRoot> {
public:
  TextDocumentRoot(const OpenDocument *, pugi::xml_node node)
      : DefaultRoot(node) {}

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const common::DocumentCursor &cursor) const final {
    return {}; // TODO
  }
};

class PresentationRoot final : public DefaultRoot<PresentationRoot> {
public:
  PresentationRoot(const OpenDocument *, pugi::xml_node node)
      : DefaultRoot(node) {}
};

class SpreadsheetRoot final : public DefaultRoot<SpreadsheetRoot> {
public:
  SpreadsheetRoot(const OpenDocument *, pugi::xml_node node)
      : DefaultRoot(node) {}
};

class DrawingRoot final : public DefaultRoot<DrawingRoot> {
public:
  DrawingRoot(const OpenDocument *, pugi::xml_node node) : DefaultRoot(node) {}
};

namespace {
template <typename Derived>
DocumentCursor::Element *
construct_default(const OpenDocument *document, pugi::xml_node node,
                  DocumentCursor::Allocator allocator) {
  auto alloc = allocator(sizeof(Derived));
  return new (alloc) Derived(document, node);
};

template <typename Derived>
DocumentCursor::Element *
construct_default_optional(const OpenDocument *document, pugi::xml_node node,
                           DocumentCursor::Allocator allocator) {
  if (!node) {
    return nullptr;
  }
  return construct_default<Derived>(document, node, allocator);
};

DocumentCursor::Element *
construct_default_element(const OpenDocument *document, pugi::xml_node node,
                          DocumentCursor::Allocator allocator) {
  using Constructor = std::function<DocumentCursor::Element *(
      const OpenDocument *document, pugi::xml_node node,
      DocumentCursor::Allocator allocator)>;

  static std::unordered_map<std::string, Constructor> constructor_table{
      {"office:text", construct_default<TextDocumentRoot>},
      {"office:presentation", construct_default<PresentationRoot>},
      {"office:spreadsheet", construct_default<SpreadsheetRoot>},
      {"office:drawing", construct_default<DrawingRoot>},
      /*{"text:p", construct_default<Paragraph>},
      {"text:h", construct_default<Paragraph>},
      {"text:span", construct_default<Span>},
      {"text:s", construct_default<Text>},
      {"text:tab", construct_default<Text>},
      {"text:line-break", construct_default<LineBreak>},
      {"text:a", construct_default<Link>},
      {"text:bookmark", construct_default<Bookmark>},
      {"text:bookmark-start", construct_default<Bookmark>},
      {"text:list", construct_default<List>},
      {"text:list-item", construct_default<ListItem>},
      {"text:index-title", construct_default<Paragraph>},
      {"text:table-of-content", construct_default<Group>},
      {"text:index-body", construct_default<Group>},
      {"table:table", construct_default<TableElement>},
      {"table:table-column", construct_default<TableColumn>},
      {"table:table-row", construct_default<TableRow>},
      {"table:table-cell", construct_default<TableCell>},
      {"draw:frame", construct_default<Frame>},
      {"draw:image", construct_default<Image>},
      {"draw:rect", construct_default<Rect>},
      {"draw:line", construct_default<Line>},
      {"draw:circle", construct_default<Circle>},
      {"draw:custom-shape", construct_default<CustomShape>},
      {"draw:text-box", construct_default<Group>},
      {"draw:g", construct_default<Group>},*/
  };

  /*
  if (node.type() == pugi::xml_node_type::node_pcdata) {
    return construct_default<Text>(document, node, allocator);
  }
   */

  if (auto constructor_it = constructor_table.find(node.name());
      constructor_it != std::end(constructor_table)) {
    return constructor_it->second(document, node, allocator);
  }

  return {};
}

DocumentCursor::Element *
construct_default_parent_element(const OpenDocument *document,
                                 pugi::xml_node node,
                                 DocumentCursor::Allocator allocator) {
  for (pugi::xml_node parent = node.parent(); parent;
       parent = parent.parent()) {
    if (auto result = construct_default_element(document, parent, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
construct_default_first_child_element(const OpenDocument *document,
                                      pugi::xml_node node,
                                      DocumentCursor::Allocator allocator) {
  for (pugi::xml_node first_child = node.first_child(); first_child;
       first_child = first_child.next_sibling()) {
    if (auto result =
            construct_default_element(document, first_child, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *construct_default_previous_sibling_element(
    const OpenDocument *document, pugi::xml_node node,
    DocumentCursor::Allocator allocator) {
  for (pugi::xml_node previous_sibling = node.previous_sibling();
       previous_sibling;
       previous_sibling = previous_sibling.previous_sibling()) {
    if (auto result =
            construct_default_element(document, previous_sibling, allocator)) {
      return result;
    }
  }
  return {};
}

DocumentCursor::Element *
construct_default_next_sibling_element(const OpenDocument *document,
                                       pugi::xml_node node,
                                       DocumentCursor::Allocator allocator) {
  for (pugi::xml_node next_sibling = node.next_sibling(); next_sibling;
       next_sibling = next_sibling.next_sibling()) {
    if (auto result =
            construct_default_element(document, next_sibling, allocator)) {
      return result;
    }
  }
  return {};
}
} // namespace

DocumentCursor::DocumentCursor(const OpenDocument *document,
                               pugi::xml_node root)
    : m_document{document} {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = construct_default_element(document, root, allocator);
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

} // namespace odr::internal::odf
