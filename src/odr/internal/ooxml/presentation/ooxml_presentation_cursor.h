#ifndef ODR_INTERNAL_OOXML_PRESENTATION_CURSOR_H
#define ODR_INTERNAL_OOXML_PRESENTATION_CURSOR_H

#include <memory>
#include <odr/internal/common/document_cursor.h>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class DocumentCursor;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml::presentation {
class Document;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;
};

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_CURSOR_H
