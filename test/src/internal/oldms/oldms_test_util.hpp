#pragma once

#include <odr/document_element.hpp>

#include <cstdint>
#include <string>

namespace odr::test::oldms {

/// Appends `value` little-endian, as the legacy binary formats store integers.
inline void append_u16(std::string &out, const std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>(value >> 8));
}

inline void append_u32(std::string &out, const std::uint32_t value) {
  append_u16(out, static_cast<std::uint16_t>(value & 0xFFFF));
  append_u16(out, static_cast<std::uint16_t>(value >> 16));
}

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
