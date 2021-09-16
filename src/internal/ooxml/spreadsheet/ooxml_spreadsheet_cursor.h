#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_CURSOR_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_CURSOR_H

#include <internal/common/document_cursor.h>

namespace odr::internal::ooxml::spreadsheet {
class Document;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_CURSOR_H
