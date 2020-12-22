#include <access/path.h>
#include <access/storage.h>
#include <common/file.h>
#include <common/string_util.h>
#include <common/xml_properties.h>
#include <common/xml_util.h>
#include <odr/document_elements.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/ooxml_elements.h>

namespace odr::ooxml {

namespace {
template <typename E, typename... Args>
std::shared_ptr<E> factorizeKnownElement(pugi::xml_node node, Args... args) {
  if (!node)
    return {};
  return std::make_shared<E>(std::forward<Args>(args)..., node);
}
} // namespace

OoxmlElement::OoxmlElement(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : m_document{std::move(document)}, m_parent{std::move(parent)}, m_node{
                                                                        node} {}

std::shared_ptr<const common::Element> OoxmlElement::parent() const {
  return m_parent;
}

std::shared_ptr<const common::Element> OoxmlElement::firstChild() const {
  return factorizeFirstChild(m_document, shared_from_this(), m_node);
}

std::shared_ptr<const common::Element> OoxmlElement::previousSibling() const {
  return factorizePreviousSibling(m_document, m_parent, m_node);
}

std::shared_ptr<const common::Element> OoxmlElement::nextSibling() const {
  return factorizeNextSibling(m_document, m_parent, m_node);
}

OoxmlRoot::OoxmlRoot(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     pugi::xml_node node)
    : OoxmlElement(std::move(document), nullptr, node) {}

ElementType OoxmlRoot::type() const { return ElementType::ROOT; }

std::shared_ptr<const common::Element> OoxmlRoot::parent() const { return {}; }

std::shared_ptr<const common::Element> OoxmlRoot::previousSibling() const {
  return {};
}

std::shared_ptr<const common::Element> OoxmlRoot::nextSibling() const {
  return {};
}

OoxmlTextDocumentRoot::OoxmlTextDocumentRoot(
    std::shared_ptr<const OfficeOpenXmlDocument> document, pugi::xml_node node)
    : OoxmlRoot(std::move(document), node) {}

std::shared_ptr<const common::Element>
OoxmlTextDocumentRoot::firstChild() const {
  return factorizeFirstChild(m_document, shared_from_this(), m_node);
}

OoxmlPresentationRoot::OoxmlPresentationRoot(
    std::shared_ptr<const OfficeOpenXmlDocument> document, pugi::xml_node node)
    : OoxmlRoot(std::move(document), node) {}

std::shared_ptr<const common::Element>
OoxmlPresentationRoot::firstChild() const {
  return std::make_shared<OoxmlSlide>(m_document, shared_from_this(),
                                      m_node.child("p:sldIdLst"));
}

OoxmlSpreadsheetRoot::OoxmlSpreadsheetRoot(
    std::shared_ptr<const OfficeOpenXmlDocument> document, pugi::xml_node node)
    : OoxmlRoot(std::move(document), node) {}

std::shared_ptr<const common::Element>
OoxmlSpreadsheetRoot::firstChild() const {
  return {}; // TODO
}

OoxmlPrimitive::OoxmlPrimitive(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node,
    const ElementType type)
    : OoxmlElement(std::move(document), std::move(parent), node), m_type{type} {
}

ElementType OoxmlPrimitive::type() const { return m_type; }

OoxmlSlide::OoxmlSlide(std::shared_ptr<const OfficeOpenXmlDocument> document,
                       std::shared_ptr<const OoxmlElement> parent,
                       pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {
  // TODO
  m_slideXml = common::XmlUtil::parse(*m_document->storage(), "");
}

std::shared_ptr<const common::Element> OoxmlSlide::firstChild() const {
  return factorizeFirstChild(
      m_document, shared_from_this(),
      m_slideXml.document_element().child("p:cSld").child("p:spTree"));
}

std::string OoxmlSlide::name() const {
  return ""; // TODO
}

std::string OoxmlSlide::notes() const {
  return ""; // TODO
}

std::shared_ptr<common::PageStyle> OoxmlSlide::pageStyle() const {
  return {}; // TODO
}

OoxmlSheet::OoxmlSheet(std::shared_ptr<const OfficeOpenXmlDocument> document,
                       std::shared_ptr<const common::Element> parent,
                       pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {
  // TODO
}

std::string OoxmlSheet::name() const {
  return ""; // TODO
}

std::uint32_t OoxmlSheet::rowCount() const {
  return 0; // TODO
}

std::uint32_t OoxmlSheet::columnCount() const {
  return 0; // TODO
}

std::shared_ptr<const common::Table> OoxmlSheet::table() const {
  return {}; // TODO;
}

OoxmlTextElement::OoxmlTextElement(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::string OoxmlTextElement::text() const {
  return m_node.first_child().value();
}

OoxmlParagraph::OoxmlParagraph(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::ParagraphStyle> OoxmlParagraph::paragraphStyle() const {
  return {}; // TODO
}

std::shared_ptr<common::TextStyle> OoxmlParagraph::textStyle() const {
  return {}; // TODO
}

OoxmlSpan::OoxmlSpan(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::TextStyle> OoxmlSpan::textStyle() const {
  return {}; // TODO
}

OoxmlLink::OoxmlLink(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::TextStyle> OoxmlLink::textStyle() const {
  return {}; // TODO
}

std::string OoxmlLink::href() const {
  return m_node.attribute("xlink:href").value();
}

OoxmlBookmark::OoxmlBookmark(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::string OoxmlBookmark::name() const {
  return m_node.attribute("text:name").value();
}

OoxmlList::OoxmlList(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<const common::Element> OoxmlList::firstChild() const {
  return factorizeKnownElement<OoxmlListItem>(m_node.child("text:list-item"),
                                              m_document, shared_from_this());
}

OoxmlListItem::OoxmlListItem(
    std::shared_ptr<const OfficeOpenXmlDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : OoxmlElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<const common::Element> OoxmlListItem::previousSibling() const {
  return factorizeKnownElement<OoxmlListItem>(
      m_node.previous_sibling("text:list-item"), m_document,
      shared_from_this());
}

std::shared_ptr<const common::Element> OoxmlListItem::nextSibling() const {
  return factorizeKnownElement<OoxmlListItem>(
      m_node.next_sibling("text:list-item"), m_document, shared_from_this());
}

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OfficeOpenXmlDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "w:t")
      return std::make_shared<OoxmlTextElement>(std::move(document),
                                                std::move(parent), node);
    if (element == "w:p")
      return std::make_shared<OoxmlParagraph>(std::move(document),
                                              std::move(parent), node);
    if (element == "w:r")
      return std::make_shared<OoxmlSpan>(std::move(document), std::move(parent),
                                         node);
    if (element == "w:hyperlink")
      return std::make_shared<OoxmlLink>(std::move(document), std::move(parent),
                                         node);

    // TODO log element
  }

  return {};
}

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OfficeOpenXmlDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    auto element = factorizeElement(document, parent, c);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OfficeOpenXmlDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

} // namespace odr::ooxml
