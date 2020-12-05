#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::common {

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType Paragraph::type() const { return ElementType::PARAGRAPH; }

ElementType Span::type() const { return ElementType::SPAN; }

ElementType Link::type() const { return ElementType::LINK; }

ElementType Bookmark::type() const { return ElementType::BOOKMARK; }

ElementType Image::type() const { return ElementType::IMAGE; }

ElementType List::type() const { return ElementType::LIST; }

ElementType ListItem::type() const { return ElementType::LIST_ITEM; }

ElementType Table::type() const { return ElementType::TABLE; }

ElementType TableColumn::type() const { return ElementType::TABLE_COLUMN; }

ElementType TableRow::type() const { return ElementType::TABLE_ROW; }

ElementType TableCell::type() const { return ElementType::TABLE_CELL; }

} // namespace odr::common
