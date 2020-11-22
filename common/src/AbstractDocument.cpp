#include <common/AbstractDocument.h>
#include <odr/Elements.h>
#include <odr/Document.h>

namespace odr::common {

std::optional<Element> AbstractElement::convert(std::shared_ptr<const common::AbstractElement> element) {
  if (!element)
    return {};
  return Element(std::move(element));
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
