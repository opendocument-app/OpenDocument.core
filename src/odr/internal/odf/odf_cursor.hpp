#ifndef ODR_INTERNAL_ODF_CURSOR_H
#define ODR_INTERNAL_ODF_CURSOR_H

#include <memory>
#include <odr/internal/common/document_cursor.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/odf/odf_style.hpp>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class DocumentCursor;
} // namespace odr::internal::abstract

namespace odr::internal::odf {
class Document;

class DocumentCursor final : public common::DocumentCursor {
public:
  DocumentCursor(const Document *document, pugi::xml_node root);

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

private:
  [[nodiscard]] common::ResolvedStyle partial_style() const final;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CURSOR_H
