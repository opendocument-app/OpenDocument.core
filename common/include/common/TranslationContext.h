#ifndef ODR_TRANSLATIONCONTEXT_H
#define ODR_TRANSLATIONCONTEXT_H

#include <access/Path.h>
#include <common/TableCursor.h>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <tinyxml2.h>
#include <unordered_map>
#include <vector>

namespace odr {
struct Config;
struct FileMeta;

namespace access {
class Storage;
}
} // namespace odr

namespace odr {
namespace common {

struct TranslationContext {
  const Config *config;
  const FileMeta *meta;

  // input files
  access::Storage *storage;

  // input xml
  std::unique_ptr<tinyxml2::XMLDocument> style;
  std::unique_ptr<tinyxml2::XMLDocument> content;
  std::unordered_map<std::string, std::list<std::string>> styleDependencies;
  std::unordered_map<std::string, std::string> msRelations;       // ooxml
  std::unique_ptr<tinyxml2::XMLDocument> msSharedStringsDocument; // xlsx
  std::vector<const tinyxml2::XMLElement *> msSharedStrings;      // xlsx

  // current
  tinyxml2::XMLNode *currentStyleNode;
  tinyxml2::XMLNode *currentContentNode;
  std::uint32_t currentEntry;
  std::uint32_t currentTableRowStart;
  std::uint32_t currentTableRowEnd;
  std::uint32_t currentTableColStart;
  std::uint32_t currentTableColEnd;
  TableCursor tableCursor;
  std::unordered_map<std::uint32_t, std::string> odDefaultCellStyles;

  // output
  std::ostream *output;

  // editing
  std::uint32_t currentTextTranslationIndex;
  std::unordered_map<std::uint32_t, const tinyxml2::XMLText *> textTranslation;
};

} // namespace common
} // namespace odr

#endif // ODR_TRANSLATIONCONTEXT_H
