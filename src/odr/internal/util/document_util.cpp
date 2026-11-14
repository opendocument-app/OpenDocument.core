#include <odr/internal/util/document_util.hpp>

namespace odr::internal {

Element util::document::first_paragraph_in(const Element &element) {
  for (const Element child : element.children()) {
    if (child.type() == ElementType::paragraph) {
      return child;
    }
    if (const Element found = first_paragraph_in(child); found) {
      return found;
    }
  }
  return {};
}

} // namespace odr::internal
