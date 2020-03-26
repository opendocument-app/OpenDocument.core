#ifndef ODR_OPENDOCUMENTCONTEXT_H
#define ODR_OPENDOCUMENTCONTEXT_H

#include <access/Path.h>
#include <access/Storage.h>
#include <common/TableCursor.h>
#include <iostream>
#include <list>
#include <memory>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <string>
#include <tinyxml2.h>
#include <unordered_map>
#include <vector>

namespace odr {

struct OpenDocumentContext {
  access::Storage *storage;

  std::unordered_map<std::string, std::list<std::string>> styleDependencies;

  std::uint32_t currentEntry;
  std::uint32_t currentTableRowStart;
  std::uint32_t currentTableRowEnd;
  std::uint32_t currentTableColStart;
  std::uint32_t currentTableColEnd;
  common::TableCursor tableCursor;
  std::unordered_map<std::uint32_t, std::string> defaultCellStyles;

  std::ostream *output;

  // editing
  std::uint32_t currentTextTranslationIndex;
  std::unordered_map<std::uint32_t, const tinyxml2::XMLText *> textTranslation;
};

} // namespace odr

#endif // ODR_OPENDOCUMENTCONTEXT_H
