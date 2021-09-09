#ifndef ODR_INTERNAL_ODF_CURSOR_H
#define ODR_INTERNAL_ODF_CURSOR_H

#include <internal/common/document_cursor.h>
#include <internal/odf/odf_style.h>

namespace odr::internal::odf {
class Document;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

  const ResolvedStyle &current_style() const;

private:
  std::vector<ResolvedStyle> m_style_stack;

  void pushed_(abstract::Element *element) final;
  void popping_(abstract::Element *element) final;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CURSOR_H
