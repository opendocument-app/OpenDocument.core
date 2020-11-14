#include <odr/GenericDocument.h>

namespace odr {

bool GenericElement::isText() const {
  return type() == Type::TEXT;
}

bool GenericElement::isLineBreak() const {
  return type() == Type::LINE_BREAK;
}

bool GenericElement::isPageBreak() const {
  return type() == Type::PAGE_BREAK;
}

bool GenericElement::isParagraph() const {
  return type() == Type::PARAGRAPH;
}

bool GenericElement::isSpan() const {
  return type() == Type::SPAN;
}

bool GenericElement::isLink() const {
  return type() == Type::LINK;
}

bool GenericElement::isImage() const {
  return type() == Type::IMAGE;
}

bool GenericElement::isList() const {
  return type() == Type::LIST;
}

bool GenericElement::isTable() const {
  return type() == Type::TABLE;
}

GenericElement::Type GenericText::type() const {
  return GenericElement::Type::TEXT;
}

GenericElement::Type GenericParagraph::type() const {
  return GenericElement::Type::PARAGRAPH;
}

GenericElement::Type GenericTable::type() const {
  return GenericElement::Type::TABLE;
}

bool GenericDocument::isText() const {
  return type() == Type::TEXT;
}

bool GenericDocument::isPresentation() const {
  return type() == Type::PRESENTATION;
}

bool GenericDocument::isSpreadsheet() const {
  return type() == Type::SPREADSHEET;
}

GenericDocument::Type GenericTextDocument::type() const {
  return Type::TEXT;
}

GenericDocument::Type GenericPresentation::type() const {
  return Type::PRESENTATION;
}

GenericDocument::Type GenericSpreadsheet::type() const {
  return Type::SPREADSHEET;
}

}
