#ifndef ODR_INTERNAL_ODF_CURSOR_H
#define ODR_INTERNAL_ODF_CURSOR_H

#include <internal/common/document_cursor.h>

namespace odr::internal::odf {
class OpenDocument;
class Style;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const OpenDocument *document, pugi::xml_node root);

private:
  const OpenDocument *m_document;

  const Style *style() const;

  static DocumentCursor::Element *
  construct_default_element(const OpenDocument *document, pugi::xml_node node,
                            const Allocator &allocator);
  static DocumentCursor::Element *
  construct_default_parent_element(const OpenDocument *document,
                                   pugi::xml_node node,
                                   const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_first_child_element(
      const OpenDocument *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_previous_sibling_element(
      const OpenDocument *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);
  static DocumentCursor::Element *construct_default_next_sibling_element(
      const OpenDocument *document, pugi::xml_node node,
      const DocumentCursor::Allocator &allocator);

  struct DefaultTraits;
  template <ElementType, typename = DefaultTraits> class DefaultElement;
  template <typename = DocumentCursor::DefaultTraits> class DefaultRoot;
  class TextDocumentRoot;
  class PresentationRoot;
  class SpreadsheetRoot;
  class DrawingRoot;
  class Slide;
  class Sheet;
  class Page;
  class Text;
  class TableElement;
  class TableColumn;
  class TableRow;
  class TableCell;
  class ImageElement;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CURSOR_H
