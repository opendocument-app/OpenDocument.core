#ifndef ODR_OOXML_ELEMENTS_H
#define ODR_OOXML_ELEMENTS_H

#include <common/document_elements.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::ooxml {
class OfficeOpenXmlDocument;

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OfficeOpenXmlDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OfficeOpenXmlDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node);

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OfficeOpenXmlDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node);

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OfficeOpenXmlDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node);

class OoxmlElement : public virtual common::Element,
                     public std::enable_shared_from_this<OoxmlElement> {
public:
  OoxmlElement(std::shared_ptr<const OfficeOpenXmlDocument> document,
               std::shared_ptr<const common::Element> parent,
               pugi::xml_node node);

  std::shared_ptr<const common::Element> parent() const override;
  std::shared_ptr<const common::Element> firstChild() const override;
  std::shared_ptr<const common::Element> previousSibling() const override;
  std::shared_ptr<const common::Element> nextSibling() const override;

protected:
  const std::shared_ptr<const OfficeOpenXmlDocument> m_document;
  const std::shared_ptr<const common::Element> m_parent;
  const pugi::xml_node m_node;
};

class OoxmlRoot : public OoxmlElement {
public:
  OoxmlRoot(std::shared_ptr<const OfficeOpenXmlDocument> document,
            pugi::xml_node node);

  ElementType type() const final;

  std::shared_ptr<const common::Element> parent() const final;
  std::shared_ptr<const common::Element> previousSibling() const final;
  std::shared_ptr<const common::Element> nextSibling() const final;
};

class OoxmlTextDocumentRoot final : public OoxmlRoot {
public:
  OoxmlTextDocumentRoot(std::shared_ptr<const OfficeOpenXmlDocument> document,
                        pugi::xml_node node);

  std::shared_ptr<const common::Element> firstChild() const final;
};

class OoxmlPresentationRoot final : public OoxmlRoot {
public:
  OoxmlPresentationRoot(std::shared_ptr<const OfficeOpenXmlDocument> document,
                        pugi::xml_node node);

  std::shared_ptr<const common::Element> firstChild() const final;
};

class OoxmlSpreadsheetRoot final : public OoxmlRoot {
public:
  OoxmlSpreadsheetRoot(std::shared_ptr<const OfficeOpenXmlDocument> document,
                       pugi::xml_node node);

  std::shared_ptr<const common::Element> firstChild() const final;
};

class OoxmlPrimitive final : public OoxmlElement {
public:
  OoxmlPrimitive(std::shared_ptr<const OfficeOpenXmlDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node, ElementType type);

  ElementType type() const final;

private:
  const ElementType m_type;
};

class OoxmlSlide final : public OoxmlElement, public common::Slide {
public:
  OoxmlSlide(std::shared_ptr<const OfficeOpenXmlDocument> document,
             std::shared_ptr<const OoxmlElement> parent, pugi::xml_node node);

  std::shared_ptr<const common::Element> firstChild() const final;

  std::string name() const final;
  std::string notes() const final;

  std::shared_ptr<common::PageStyle> pageStyle() const final;

private:
  pugi::xml_document m_slideXml;
};

class OoxmlSheet final : public OoxmlElement, public common::Sheet {
public:
  OoxmlSheet(std::shared_ptr<const OfficeOpenXmlDocument> document,
             std::shared_ptr<const common::Element> parent,
             pugi::xml_node node);

  std::string name() const final;
  std::uint32_t rowCount() const final;
  std::uint32_t columnCount() const final;
  std::shared_ptr<const common::Table> table() const final;

private:
  pugi::xml_document m_sheetXml;
};

class OoxmlTextElement final : public OoxmlElement, public common::TextElement {
public:
  OoxmlTextElement(std::shared_ptr<const OfficeOpenXmlDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node);

  std::string text() const final;
};

class OoxmlParagraph final : public OoxmlElement, public common::Paragraph {
public:
  OoxmlParagraph(std::shared_ptr<const OfficeOpenXmlDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node);

  std::shared_ptr<common::ParagraphStyle> paragraphStyle() const final;
  std::shared_ptr<common::TextStyle> textStyle() const final;
};

class OoxmlSpan final : public OoxmlElement, public common::Span {
public:
  OoxmlSpan(std::shared_ptr<const OfficeOpenXmlDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node);

  std::shared_ptr<common::TextStyle> textStyle() const final;
};

class OoxmlLink final : public OoxmlElement, public common::Link {
public:
  OoxmlLink(std::shared_ptr<const OfficeOpenXmlDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node);

  std::shared_ptr<common::TextStyle> textStyle() const final;

  std::string href() const final;
};

class OoxmlBookmark final : public OoxmlElement, public common::Bookmark {
public:
  OoxmlBookmark(std::shared_ptr<const OfficeOpenXmlDocument> document,
                std::shared_ptr<const common::Element> parent,
                pugi::xml_node node);

  std::string name() const final;
};

class OoxmlList final : public OoxmlElement, public common::List {
public:
  OoxmlList(std::shared_ptr<const OfficeOpenXmlDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node);

  std::shared_ptr<const common::Element> firstChild() const final;
};

class OoxmlListItem final : public OoxmlElement, public common::ListItem {
public:
  OoxmlListItem(std::shared_ptr<const OfficeOpenXmlDocument> document,
                std::shared_ptr<const common::Element> parent,
                pugi::xml_node node);

  std::shared_ptr<const common::Element> previousSibling() const final;
  std::shared_ptr<const common::Element> nextSibling() const final;
};

} // namespace odr::ooxml

#endif // ODR_OOXML_ELEMENTS_H
