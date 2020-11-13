#ifndef ODR_OOXML_CONTEXT_H
#define ODR_OOXML_CONTEXT_H

#include <common/TableCursor.h>
#include <common/TableRange.h>
#include <iostream>
#include <list>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr {
struct Config;
struct FileMeta;

namespace access {
class ReadStorage;
}
} // namespace odr

namespace odr {
namespace ooxml {

struct Context {
  const Config *config;
  const FileMeta *meta;

  const access::ReadStorage *storage;

  std::ostream *output;

  std::unordered_map<std::string, std::list<std::string>> styleDependencies;
  std::unordered_map<std::string, std::string> relations;
  pugi::xml_document sharedStringsDocument;  // xlsx
  std::vector<pugi::xml_node> sharedStrings; // xlsx

  std::uint32_t entry{0};
  common::TableRange tableRange;
  common::TableCursor tableCursor;
  std::unordered_map<std::uint32_t, std::string> defaultCellStyles;

  // editing
  std::uint32_t currentTextTranslationIndex{0};
  std::unordered_map<std::uint32_t, pugi::xml_text> textTranslation;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_CONTEXT_H
