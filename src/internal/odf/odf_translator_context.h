#ifndef ODR_INTERNAL_ODF_TRANSLATOR_CONTEXT_H
#define ODR_INTERNAL_ODF_TRANSLATOR_CONTEXT_H

#include <internal/common/table_cursor.h>
#include <internal/common/table_range.h>
#include <iostream>
#include <list>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace odr {
struct HtmlConfig;
struct FileMeta;
} // namespace odr

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::odf {

struct Context {
  const HtmlConfig *config;
  const FileMeta *meta;

  const abstract::ReadableFilesystem *filesystem;

  std::ostream *output;

  std::unordered_map<std::string, std::list<std::string>> style_dependencies;

  std::uint32_t entry{0};
  common::TableRange table_range;
  common::TableCursor table_cursor;
  std::unordered_map<std::uint32_t, std::string> default_cell_styles;

  // editing
  std::uint32_t current_text_translation_index{0};
  std::unordered_map<std::uint32_t, pugi::xml_text> text_translation;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_TRANSLATOR_CONTEXT_H
