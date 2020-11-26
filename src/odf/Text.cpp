#include <odf/Text.h>
#include <odf/Common.h>
#include <common/DocumentElements.h>
#include <odr/DocumentElements.h>
#include <odr/Document.h>

namespace odr::odf {

namespace {
class OdfElement;

std::shared_ptr<common::Element>
firstChildImpl(std::shared_ptr<const OdfElement> parent, pugi::xml_node node);
std::shared_ptr<common::Element>
previousSiblingImpl(std::shared_ptr<const OdfElement> parent, pugi::xml_node node);
std::shared_ptr<common::Element>
nextSiblingImpl(std::shared_ptr<const OdfElement> parent, pugi::xml_node node);

class OdfElement : public virtual common::Element,
                public std::enable_shared_from_this<OdfElement> {
public:
  OdfElement(std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : m_parent{std::move(parent)}, m_node{node} {}

  std::shared_ptr<const Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const Element> firstChild() const override {
    return firstChildImpl(shared_from_this(), m_node);
  }

  std::shared_ptr<const Element> previousSibling() const override {
    return previousSiblingImpl(m_parent, m_node);
  }

  std::shared_ptr<const Element> nextSibling() const override {
    return nextSiblingImpl(m_parent, m_node);
  }

protected:
  const std::shared_ptr<const OdfElement> m_parent;
  const pugi::xml_node m_node;
};

class OdfPrimitive final : public OdfElement {
public:
  OdfPrimitive(std::shared_ptr<const OdfElement> parent, pugi::xml_node node,
         const ElementType type)
      : OdfElement(std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class OdfTextElement : public OdfElement, public common::TextElement {
public:
  OdfTextElement(std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(parent), node) {}

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

class OdfParagraph : public OdfElement, public common::Paragraph {
public:
  OdfParagraph(std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(parent), node) {}
};

std::shared_ptr<common::Element> convert(std::shared_ptr<const OdfElement> parent,
                                        pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata) {
    return std::make_shared<OdfTextElement>(std::move(parent), node);
  }

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h")
      return std::make_shared<OdfParagraph>(std::move(parent), node);
    else if (element == "text:s" || element == "text:tab")
      return std::make_shared<OdfTextElement>(std::move(parent), node);
    else if (element == "text:line-break")
      return std::make_shared<OdfPrimitive>(std::move(parent), node,
                                         ElementType::LINE_BREAK);
    // else if (element == "text:a")
    //  LinkTranslator(in, out, context);
    // else if (element == "text:bookmark" || element == "text:bookmark-start")
    //  BookmarkTranslator(in, out, context);
    // else if (element == "draw:frame" || element == "draw:custom-shape")
    //  FrameTranslator(in, out, context);
    // else if (element == "draw:image")
    //  ImageTranslator(in, out, context);
    // else if (element == "table:table")
    //  TableTranslator(in, out, context);

    return std::make_shared<OdfPrimitive>(std::move(parent), node,
                                       ElementType::UNKNOWN);
  }

  return nullptr;
}

bool isSkipper(pugi::xml_node node) {
  const std::string element = node.name();

  if (element == "office:forms")
    return true;
  if (element == "text:sequence-decls")
    return true;

  return false;
}

std::shared_ptr<common::Element>
firstChildImpl(std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  for (auto &&c : node) {
    if (isSkipper(c))
      continue;
    return convert(std::move(parent), c);
  }
  return nullptr;
}

std::shared_ptr<common::Element>
previousSiblingImpl(std::shared_ptr<const OdfElement> parent,
                    pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    if (isSkipper(s))
      continue;
    return convert(std::move(parent), s);
  }
  return nullptr;
}

std::shared_ptr<common::Element>
nextSiblingImpl(std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = node.next_sibling()) {
    if (isSkipper(s))
      continue;
    return convert(std::move(parent), s);
  }
  return nullptr;
}
} // namespace

Text::Text(pugi::xml_document content,
                                   pugi::xml_document style)
    : m_content{std::move(content)}, m_style{std::move(style)} {}

pugi::xml_document &Text::content() { return m_content; }

pugi::xml_document &Text::style() { return m_style; }

PageProperties Text::pageProperties() const {
  return Common::pageProperties(m_style);
}

ElementSiblingRange Text::content() const {
  const pugi::xml_node body = m_content.child("office:document-content")
                                  .child("office:body")
                                  .child("office:text");
  return ElementSiblingRange(Element(firstChildImpl(nullptr, body)));
}

} // namespace odr::odf
