#include <common/AbstractDocument.h>

namespace odr::common {

bool AbstractElement::isUnknown() const {
  return type() == Type::UNKNOWN;
}

bool AbstractElement::isText() const {
  return type() == Type::TEXT;
}

bool AbstractElement::isLineBreak() const {
  return type() == Type::LINE_BREAK;
}

bool AbstractElement::isPageBreak() const {
  return type() == Type::PAGE_BREAK;
}

bool AbstractElement::isParagraph() const {
  return type() == Type::PARAGRAPH;
}

bool AbstractElement::isSpan() const {
  return type() == Type::SPAN;
}

bool AbstractElement::isLink() const {
  return type() == Type::LINK;
}

bool AbstractElement::isImage() const {
  return type() == Type::IMAGE;
}

bool AbstractElement::isList() const {
  return type() == Type::LIST;
}

bool AbstractElement::isTable() const {
  return type() == Type::TABLE;
}

AbstractElement::Type AbstractText::type() const {
  return AbstractElement::Type::TEXT;
}

AbstractElement::Type AbstractParagraph::type() const {
  return AbstractElement::Type::PARAGRAPH;
}

AbstractElement::Type AbstractTable::type() const {
  return AbstractElement::Type::TABLE;
}

bool AbstractDocument::isText() const {
  return type() == Type::TEXT;
}

bool AbstractDocument::isPresentation() const {
  return type() == Type::PRESENTATION;
}

bool AbstractDocument::isSpreadsheet() const {
  return type() == Type::SPREADSHEET;
}

AbstractDocument::Type AbstractTextDocument::type() const {
  return Type::TEXT;
}

AbstractDocument::Type AbstractPresentation::type() const {
  return Type::PRESENTATION;
}

AbstractDocument::Type AbstractSpreadsheet::type() const {
  return Type::SPREADSHEET;
}

}
