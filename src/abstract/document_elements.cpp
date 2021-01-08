#include <abstract/document_elements.h>
#include <odr/document_elements.h>

namespace odr::abstract {

ElementType Slide::type() const { return ElementType::SLIDE; }

ElementType Sheet::type() const { return ElementType::SHEET; }

ElementType Page::type() const { return ElementType::PAGE; }

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType Paragraph::type() const { return ElementType::PARAGRAPH; }

ElementType Span::type() const { return ElementType::SPAN; }

ElementType Link::type() const { return ElementType::LINK; }

ElementType Bookmark::type() const { return ElementType::BOOKMARK; }

ElementType List::type() const { return ElementType::LIST; }

ElementType ListItem::type() const { return ElementType::LIST_ITEM; }

ElementType Table::type() const { return ElementType::TABLE; }

ElementType TableColumn::type() const { return ElementType::TABLE_COLUMN; }

ElementType TableRow::type() const { return ElementType::TABLE_ROW; }

ElementType TableCell::type() const { return ElementType::TABLE_CELL; }

ElementType Frame::type() const { return ElementType::FRAME; }

ElementType Image::type() const { return ElementType::IMAGE; }

ElementType Rect::type() const { return ElementType::RECT; }

ElementType Line::type() const { return ElementType::LINE; }

ElementType Circle::type() const { return ElementType::CIRCLE; }

} // namespace odr::abstract
