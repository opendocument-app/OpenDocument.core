#ifndef ODR_INTERNAL_OOXML_TRANSLATOR_CONTEXT_H
#define ODR_INTERNAL_OOXML_TRANSLATOR_CONTEXT_H

#include <internal/common/table_cursor.h>
#include <internal/common/table_range.h>
#include <iostream>
#include <list>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr {
struct HtmlConfig;
} // namespace odr

namespace odr::experimental {
struct FileMeta;
}

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

struct Context {
  const HtmlConfig *config;
  const experimental::FileMeta *meta;

  const abstract::ReadableFilesystem *filesystem;

  std::ostream *output;

  std::unordered_map<std::string, std::list<std::string>> style_dependencies;
  std::unordered_map<std::string, std::string> relations;
  std::vector<pugi::xml_node> shared_strings; // xlsx

  std::uint32_t entry{0};
  common::TableRange table_range;
  common::TableCursor table_cursor;

  // editing
  std::uint32_t current_text_translation_index{0};
  std::unordered_map<std::uint32_t, pugi::xml_text> text_translation;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_TRANSLATOR_CONTEXT_H
