#include <odf/Elements.h>
#include <common/DocumentElements.h>
#include <pugixml.hpp>
#include <odf/OpenDocument.h>

namespace odr::odf {

namespace {
class OdfElement;

template <typename E>
std::shared_ptr<E> knownElementImpl(std::shared_ptr<const OpenDocument> document,
                                    std::shared_ptr<const OdfElement> parent,
                                    pugi::xml_node node) {
  if (!node)
    return {};
  return std::make_shared<E>(std::move(document), std::move(parent), node);
}

class OdfElement : public virtual common::Element,
                   public std::enable_shared_from_this<OdfElement> {
public:
  OdfElement(std::shared_ptr<const OpenDocument> document,
             std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : m_document{std::move(document)}, m_parent{std::move(parent)}, m_node{node} {}

  std::shared_ptr<const common::Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const common::Element> firstChild() const override {
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const common::Element> previousSibling() const override {
    return factorizePreviousSibling(m_document, m_parent, m_node);
  }

  std::shared_ptr<const common::Element> nextSibling() const override {
    return factorizeNextSibling(m_document, m_parent, m_node);
  }

  ParagraphProperties paragraphProperties() const {
    if (auto styleAttr = m_node.attribute("text:style-name"); styleAttr) {
      auto style = m_document->styles().style(styleAttr.value());
      if (style)
        return style->resolve().toParagraphProperties();
      // TODO log
    }
    return {};
  }

  TextProperties textProperties() const {
    if (auto styleAttr = m_node.attribute("text:style-name"); styleAttr) {
      auto style = m_document->styles().style(styleAttr.value());
      if (style)
        return style->resolve().toTextProperties();
      // TODO log
    }
    return {};
  }

protected:
  const std::shared_ptr<const OpenDocument> m_document;
  const std::shared_ptr<const common::Element> m_parent;
  const pugi::xml_node m_node;
};

class OdfPrimitive final : public OdfElement {
public:
  OdfPrimitive(std::shared_ptr<const OpenDocument> document,
               std::shared_ptr<const common::Element> parent, pugi::xml_node node,
               const ElementType type)
      : OdfElement(std::move(document), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class OdfTextElement final : public OdfElement, public common::TextElement {
public:
  OdfTextElement(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::string text() const override {
    if (m_node.type() == pugi::node_pcdata) {
      return m_node.text().as_string();
    }

    const std::string element = m_node.name();
    if (element == "text:s") {
      const auto count = m_node.attribute("text:c").as_uint(1);
      return std::string(count, ' ');
    } else if (element == "text:tab") {
      return "\t";
    }

    // TODO this should never happen. log or throw?
    return "";
  }
};

class OdfParagraph final : public OdfElement, public common::Paragraph {
public:
  OdfParagraph(std::shared_ptr<const OpenDocument> document,
               std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  ParagraphProperties paragraphProperties() const final {
    return OdfElement::paragraphProperties();
  }

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }
};

class OdfSpan final : public OdfElement, public common::Span {
public:
  OdfSpan(std::shared_ptr<const OpenDocument> document,
          std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }
};

class OdfLink final : public OdfElement, public common::Link {
public:
  OdfLink(std::shared_ptr<const OpenDocument> document,
          std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class OdfTableOfContent final : public OdfElement {
public:
  OdfTableOfContent(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  ElementType type() const final { return ElementType::UNKNOWN; }

  std::shared_ptr<const common::Element> firstChild() const override {
    return factorizeFirstChild(m_document, shared_from_this(),
                               m_node.child("text:index-body"));
  }
};

class OdfBookmark final : public OdfElement, public common::Bookmark {
public:
  OdfBookmark(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("text:name").value();
  }
};

class OdfListItem final : public OdfElement, public common::ListItem {
public:
  OdfListItem(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfListItem>(
        m_document, shared_from_this(), m_node.previous_sibling("text:list-item"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfListItem>(m_document, shared_from_this(),
                                         m_node.next_sibling("text:list-item"));
  }
};

class OdfList final : public OdfElement, public common::List {
public:
  OdfList(std::shared_ptr<const OpenDocument> document,
          std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return knownElementImpl<OdfListItem>(m_document, shared_from_this(),
                                         m_node.child("text:list-item"));
  }
};

class OdfTableColumn final : public OdfElement, public common::TableColumn {
public:
  OdfTableColumn(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final { return {}; }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfListItem>(
        m_document, shared_from_this(),
        m_node.previous_sibling("table:table-column"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfListItem>(
        m_document, shared_from_this(), m_node.next_sibling("table:table-column"));
  }
};

class OdfTableCell final : public OdfElement, public common::TableCell {
public:
  OdfTableCell(std::shared_ptr<const OpenDocument> document,
               std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfTableCell>(
        m_document, shared_from_this(),
        m_node.previous_sibling("table:table-cell"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfTableCell>(
        m_document, shared_from_this(), m_node.next_sibling("table:table-cell"));
  }

  std::uint32_t rowSpan() const final {
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t columnSpan() const final {
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }
};

class OdfTableRow final : public OdfElement, public common::TableRow {
public:
  OdfTableRow(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return knownElementImpl<OdfTableCell>(m_document, shared_from_this(),
                                          m_node.child("table:table-cell"));
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfTableRow>(
        m_document, shared_from_this(), m_node.previous_sibling("table:table-row"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfTableRow>(
        m_document, shared_from_this(), m_node.next_sibling("table:table-row"));
  }
};

class OdfTable final : public OdfElement, public common::Table {
public:
  OdfTable(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : OdfElement(std::move(document), std::move(parent), node) {}

  std::uint32_t rowCount() const final {
    return 0; // TODO
  }

  std::uint32_t columnCount() const final {
    return 0; // TODO
  }

  std::shared_ptr<const common::Element> firstChild() const final { return {}; }

  std::shared_ptr<const common::TableColumn> firstColumn() const final {
    return knownElementImpl<OdfTableColumn>(m_document, shared_from_this(),
                                            m_node.child("table:table-column"));
  }

  std::shared_ptr<const common::TableRow> firstRow() const final {
    return knownElementImpl<OdfTableRow>(m_document, shared_from_this(),
                                         m_node.child("table:table-row"));
  }
};

bool isSkipper(pugi::xml_node node) {
  // TODO this method should be removed at some point
  const std::string element = node.name();

  if (element == "office:forms")
    return true;
  if (element == "text:sequence-decls")
    return true;
  if (element == "text:bookmark-end")
    return true;

  return false;
}
}

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata) {
    return std::make_shared<OdfTextElement>(std::move(document), std::move(parent),
                                            node);
  } else if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h")
      return std::make_shared<OdfParagraph>(std::move(document), std::move(parent),
                                            node);
    else if (element == "text:span")
      return std::make_shared<OdfSpan>(std::move(document), std::move(parent),
                                       node);
    else if (element == "text:s" || element == "text:tab")
      return std::make_shared<OdfTextElement>(std::move(document),
                                              std::move(parent), node);
    else if (element == "text:line-break")
      return std::make_shared<OdfPrimitive>(std::move(document), std::move(parent),
                                            node, ElementType::LINE_BREAK);
    else if (element == "text:a")
      return std::make_shared<OdfLink>(std::move(document), std::move(parent),
                                       node);
    else if (element == "text:table-of-content")
      return std::make_shared<OdfTableOfContent>(std::move(document),
                                                 std::move(parent), node);
    else if (element == "text:bookmark" || element == "text:bookmark-start")
      return std::make_shared<OdfBookmark>(std::move(document), std::move(parent),
                                           node);
    else if (element == "text:list")
      return std::make_shared<OdfList>(std::move(document), std::move(parent),
                                       node);
    else if (element == "table:table")
      return std::make_shared<OdfTable>(std::move(document), std::move(parent),
                                        node);

    // else if (element == "draw:frame" || element == "draw:custom-shape")
    //  FrameTranslator(in, out, context);
    // else if (element == "draw:image")
    //  ImageTranslator(in, out, context);

    // TODO this should be removed at some point
    return std::make_shared<OdfPrimitive>(std::move(document), std::move(parent),
                                          node, ElementType::UNKNOWN);
  }

  return nullptr;
}

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    if (isSkipper(c))
      continue;
    return factorizeElement(std::move(document), std::move(parent), c);
  }
  return {};
}

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    if (isSkipper(s))
      continue;
    return factorizeElement(std::move(document), std::move(parent), s);
  }
  return {};
}

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OpenDocument> document,
                std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    if (isSkipper(s))
      continue;
    return factorizeElement(std::move(document), std::move(parent), s);
  }
  return {};
}

}
