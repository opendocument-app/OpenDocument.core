#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_element.h>

namespace odr::internal::odf {

DocumentCursor::DocumentCursor(const Document *document, pugi::xml_node root)
    : common::DocumentCursor(document) {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = construct_default_element(
      document, root,
      reinterpret_cast<const abstract::Allocator *>(&allocator));
  if (!element) {
    throw std::invalid_argument("root element invalid");
  }
}

} // namespace odr::internal::odf
