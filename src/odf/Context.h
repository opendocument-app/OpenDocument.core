#ifndef ODR_ODF_CONTEXT_H
#define ODR_ODF_CONTEXT_H

#include <common/TableCursor.h>
#include <common/TableRange.h>
#include <iostream>
#include <list>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_text;
}

namespace odr {
struct Config;
struct FileMeta;

namespace access {
class ReadStorage;
}
} // namespace odr

namespace odr::odf {

struct Context {
  const Config *config;
  const FileMeta *meta;

  const access::ReadStorage *storage;

  std::ostream *output;

  std::unordered_map<std::string, std::list<std::string>> styleDependencies;

  std::uint32_t entry{0};
  common::TableRange tableRange;
  common::TableCursor tableCursor;
  std::unordered_map<std::uint32_t, std::string> defaultCellStyles;

  // editing
  std::uint32_t currentTextTranslationIndex{0};
  std::unordered_map<std::uint32_t, pugi::xml_text> textTranslation;
};

} // namespace odr::odf

#endif // ODR_ODF_CONTEXT_H
