#ifndef ODR_INTERNAL_ODF_CURSOR_H
#define ODR_INTERNAL_ODF_CURSOR_H

#include <internal/common/document_cursor.h>

namespace odr::internal::odf {
class OpenDocument;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const OpenDocument *document, pugi::xml_node root);

  template <typename Derived, ElementType element_type> class DefaultElement;

private:
  const OpenDocument *m_document;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CURSOR_H
