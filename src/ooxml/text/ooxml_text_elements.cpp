#include <access/path.h>
#include <access/storage.h>
#include <common/file.h>
#include <common/string_util.h>
#include <common/xml_properties.h>
#include <odr/document_elements.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/text/ooxml_text_elements.h>

namespace odr::ooxml::text {

namespace {
template <typename E, typename... Args>
std::shared_ptr<E> factorizeKnownElement(pugi::xml_node node, Args... args) {
  if (!node)
    return {};
  return std::make_shared<E>(std::forward<Args>(args)..., node);
}

class Element : public virtual common::Element,
                public std::enable_shared_from_this<Element> {
public:
  Element(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
          std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : m_document{std::move(document)}, m_parent{std::move(parent)},
        m_node{node} {}

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

protected:
  const std::shared_ptr<const OfficeOpenXmlTextDocument> m_document;
  const std::shared_ptr<const common::Element> m_parent;
  const pugi::xml_node m_node;
};

class Root final : public Element {
public:
  Root(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       pugi::xml_node node)
      : Element(std::move(document), nullptr, node) {}

  ElementType type() const final { return ElementType::ROOT; }

  std::shared_ptr<const common::Element> parent() const final { return {}; }

  std::shared_ptr<const common::Element> firstChild() const final {
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return {};
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return {};
  }
};

class Primitive final : public Element {
public:
  Primitive(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node,
            const ElementType type)
      : Element(std::move(document), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class TextElement final : public Element, public common::TextElement {
public:
  TextElement(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              std::shared_ptr<const common::Element> parent,
              pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string text() const final { return m_node.first_child().value(); }
};

class Paragraph final : public Element, public common::Paragraph {
public:
  Paragraph(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::ParagraphStyle> paragraphStyle() const final {
    return m_document->styles().paragraphStyle(m_node.child("w:pPr"));
  }

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return m_document->styles().textStyle();
  }
};

class Span final : public Element, public common::Span {
public:
  Span(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return m_document->styles().textStyle();
  }
};

class Link final : public Element, public common::Link {
public:
  Link(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return m_document->styles().textStyle();
  }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public common::Bookmark {
public:
  Bookmark(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
           std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final { return m_node.attribute("w:name").value(); }
};

class ListItem final : public Element, public common::ListItem {
public:
  ListItem(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
           std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.previous_sibling("text:list-item"), m_document,
        shared_from_this());
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.next_sibling("text:list-item"), m_document, shared_from_this());
  }
};

class List final : public Element, public common::List {
public:
  List(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return factorizeKnownElement<ListItem>(m_node.child("text:list-item"),
                                           m_document, shared_from_this());
  }
};
} // namespace

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "w:t")
      return std::make_shared<TextElement>(std::move(document),
                                           std::move(parent), node);
    if (element == "w:p")
      return std::make_shared<Paragraph>(std::move(document), std::move(parent),
                                         node);
    if (element == "w:r")
      return std::make_shared<Span>(std::move(document), std::move(parent),
                                    node);
    if (element == "w:hyperlink")
      return std::make_shared<Link>(std::move(document), std::move(parent),
                                    node);
    if (element == "w:bookmarkStart")
      return std::make_shared<Bookmark>(std::move(document), std::move(parent),
                                        node);

    // TODO log element
  }

  return {};
}

std::shared_ptr<common::Element>
factorizeRoot(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              pugi::xml_node node) {
  return std::make_shared<Root>(std::move(document), node);
}

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    auto element = factorizeElement(document, parent, c);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element> factorizePreviousSibling(
    std::shared_ptr<const OfficeOpenXmlTextDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

} // namespace odr::ooxml::text
