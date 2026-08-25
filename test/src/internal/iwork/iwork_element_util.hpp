#pragma once

#include <odr/document_element.hpp>

#include <string>

/// Reading an iWork document back out, for the tests that assert what one
/// decodes to. The sibling `iwork_test_util.hpp` assembles the input; this
/// reads the output.
namespace odr::test::iwork {

/// The text of one element, a line break reading as a newline and a paragraph
/// boundary as well. The engine emits a `line_break` only where a storage
/// carried a line separator inside a paragraph, so both really are newlines.
inline std::string element_text(const Element element) {
  std::string result;
  for (const Element paragraph : element.children()) {
    if (!result.empty()) {
      result += '\n';
    }
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
