#ifndef ODR_OPENDOCUMENTCONTEXT_H
#define ODR_OPENDOCUMENTCONTEXT_H

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "access/Path.h"
#include "access/Storage.h"
#include "common/TableCursor.h"
#include "../../../odr/include/odr/Config.h" // TODO
#include "../../../odr/include/odr/Meta.h" // TODO
#include "tinyxml2.h"

namespace odr {

struct OpenDocumentContext {
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
