#ifndef ODR_INTERNAL_OOXML_TEXT_CURSOR_H
#define ODR_INTERNAL_OOXML_TEXT_CURSOR_H

#include <internal/common/document_cursor.h>

namespace odr::internal::ooxml::text {
class Document;
class Style;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_CURSOR_H
