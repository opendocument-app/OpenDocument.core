#ifndef ODR_OPENDOCUMENTCONTEXT_H
#define ODR_OPENDOCUMENTCONTEXT_H

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "../io/Path.h"
#include "../io/Storage.h"
#include "../TableCursor.h"
#include "odr/Config.h"
#include "odr/Meta.h"
#include "tinyxml2.h"

namespace odr {

struct OpenDocumentContext {
  const Config *config;
  const FileMeta *meta;

  Storage *storage;

  std::unordered_map<std::string, std::list<std::string>> styleDependencies;

  std::uint32_t currentEntry;
  std::uint32_t currentTableRowStart;
  std::uint32_t currentTableRowEnd;
  std::uint32_t currentTableColStart;
  std::uint32_t currentTableColEnd;
  TableCursor tableCursor;
  std::unordered_map<std::uint32_t, std::string> defaultCellStyles;

  std::ostream *output;

  // editing
  std::uint32_t currentTextTranslationIndex;
  std::unordered_map<std::uint32_t, const tinyxml2::XMLText *> textTranslation;
};

}

#endif // ODR_OPENDOCUMENTCONTEXT_H
