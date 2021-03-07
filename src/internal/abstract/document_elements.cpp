#include <internal/abstract/document_elements.h>
#include <odr/experimental/element_type.h>

namespace odr::internal::abstract {

experimental::ElementType Slide::type() const {
  return experimental::ElementType::SLIDE;
}

experimental::ElementType Sheet::type() const {
  return experimental::ElementType::SHEET;
}

experimental::ElementType Page::type() const {
  return experimental::ElementType::PAGE;
}

experimental::ElementType TextElement::type() const {
  return experimental::ElementType::TEXT;
}

experimental::ElementType Paragraph::type() const {
  return experimental::ElementType::PARAGRAPH;
}

experimental::ElementType Span::type() const {
  return experimental::ElementType::SPAN;
}

experimental::ElementType Link::type() const {
  return experimental::ElementType::LINK;
}

experimental::ElementType Bookmark::type() const {
  return experimental::ElementType::BOOKMARK;
}

experimental::ElementType List::type() const {
  return experimental::ElementType::LIST;
}

experimental::ElementType ListItem::type() const {
  return experimental::ElementType::LIST_ITEM;
}

experimental::ElementType Table::type() const {
  return experimental::ElementType::TABLE;
}

experimental::ElementType TableColumn::type() const {
  return experimental::ElementType::TABLE_COLUMN;
}

experimental::ElementType TableRow::type() const {
  return experimental::ElementType::TABLE_ROW;
}

experimental::ElementType TableCell::type() const {
  return experimental::ElementType::TABLE_CELL;
}

experimental::ElementType Frame::type() const {
  return experimental::ElementType::FRAME;
}

experimental::ElementType Image::type() const {
  return experimental::ElementType::IMAGE;
}

experimental::ElementType Rect::type() const {
  return experimental::ElementType::RECT;
}

experimental::ElementType Line::type() const {
  return experimental::ElementType::LINE;
}

experimental::ElementType Circle::type() const {
  return experimental::ElementType::CIRCLE;
}

} // namespace odr::internal::abstract
