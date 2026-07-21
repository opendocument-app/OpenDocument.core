#pragma once

#include <odr/document_element.hpp>

#include <string>

namespace odr::test::oldms {

/// Collects the text content of an element subtree, joining paragraphs with
/// newlines so the paragraph structure is observable.
inline std::string collect_text(const Element element) {
  if (element.type() == ElementType::text) {
    return element.as_text().content();
  }
  std::string result;
  bool first = true;
  for (const Element child : element.children()) {
    if (!first && child.type() == ElementType::paragraph) {
      result += '\n';
    }
    result += collect_text(child);
    first = false;
  }
  return result;
}

} // namespace odr::test::oldms
