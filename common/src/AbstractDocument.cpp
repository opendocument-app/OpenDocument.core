#include <common/AbstractDocument.h>
#include <odr/Elements.h>
#include <odr/Document.h>

namespace odr::common {

bool AbstractElement::isUnknown() const {
  return type() == ElementType::UNKNOWN;
}

bool AbstractElement::isText() const {
  return type() == ElementType::TEXT;
}

bool AbstractElement::isLineBreak() const {
  return type() == ElementType::LINE_BREAK;
}

bool AbstractElement::isPageBreak() const {
  return type() == ElementType::PAGE_BREAK;
}

bool AbstractElement::isParagraph() const {
  return type() == ElementType::PARAGRAPH;
}

bool AbstractElement::isSpan() const {
  return type() == ElementType::SPAN;
}

bool AbstractElement::isLink() const {
  return type() == ElementType::LINK;
}

bool AbstractElement::isImage() const {
  return type() == ElementType::IMAGE;
}

bool AbstractElement::isList() const {
  return type() == ElementType::LIST;
}

bool AbstractElement::isTable() const {
  return type() == ElementType::TABLE;
}

ElementType AbstractText::type() const {
  return ElementType::TEXT;
}

ElementType AbstractParagraph::type() const {
  return ElementType::PARAGRAPH;
}

ElementType AbstractTable::type() const {
  return ElementType::TABLE;
}

bool AbstractDocument::isText() const {
  return type() == DocumentType::TEXT;
}

bool AbstractDocument::isPresentation() const {
  return type() == DocumentType::PRESENTATION;
}

bool AbstractDocument::isSpreadsheet() const {
  return type() == DocumentType::SPREADSHEET;
}

DocumentType AbstractTextDocument::type() const {
  return DocumentType::TEXT;
}

DocumentType AbstractPresentation::type() const {
  return DocumentType::PRESENTATION;
}

DocumentType AbstractSpreadsheet::type() const {
  return DocumentType::SPREADSHEET;
}

}
