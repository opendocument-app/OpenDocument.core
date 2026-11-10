#pragma once

#include <odr/document_element.hpp>

namespace odr::internal {

/// Whether @p element, a child of a text root, is anchored to the page rather
/// than laid out in the text: a frame or shape, or a link around them.
inline bool is_page_anchored(const Element &element) {
  if (element.type() == ElementType::frame) {
    return true;
  }
  if (element.type() != ElementType::link || !element.first_child()) {
    return false;
  }
  for (const Element child : element.children()) {
    if (child.type() != ElementType::frame) {
      return false;
    }
  }
  return true;
}

} // namespace odr::internal
