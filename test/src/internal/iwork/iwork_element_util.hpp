#pragma once

#include <odr/document_element.hpp>

#include <string>

/// Helpers for inspecting decoded iWork test documents.
namespace odr::test::iwork {

/// Joins paragraphs and line breaks with newlines, preserving empty paragraphs.
inline std::string element_text(const Element element) {
  std::string result;
  bool first = true;
  for (const Element paragraph : element.children()) {
    if (!first) {
      result += '\n';
    }
    first = false;
    for (const Element child : paragraph.children()) {
      if (child.type() == ElementType::line_break) {
        result += '\n';
      } else {
        result += child.as_text().content();
      }
    }
  }
  return result;
}

} // namespace odr::test::iwork
