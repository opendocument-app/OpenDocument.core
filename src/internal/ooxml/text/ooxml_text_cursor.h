#ifndef ODR_INTERNAL_OOXML_TEXT_CURSOR_H
#define ODR_INTERNAL_OOXML_TEXT_CURSOR_H

#include <internal/common/document_cursor.h>
#include <internal/common/style.h>
#include <memory>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class DocumentCursor;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml::text {
class Document;
class Style;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

private:
  common::ResolvedStyle partial_style() const final;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_CURSOR_H
