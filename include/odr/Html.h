#ifndef ODR_HTML_H
#define ODR_HTML_H

#include <cstdint>
#include <string>

namespace odr {
class Document;

namespace Html {

enum class TableGridlines {
  NONE,
  SOFT,
  HARD,
};

struct Config {
  // starting sheet for spreadsheet, starting page for presentation, ignored for
  // text, ignored for graphics
  std::uint32_t entryOffset{0};
  // translate only N sheets / pages; zero means translate all
  std::uint32_t entryCount{0};
  // create output for each entry
  bool splitEntries{false};
  // create editable output
  bool editable{false};

  // spreadsheet table offset
  std::uint32_t tableOffsetRows{0};
  std::uint32_t tableOffsetCols{0};
  // spreadsheet table limit
  std::uint32_t tableLimitRows{10000};
  std::uint32_t tableLimitCols{500};
  bool tableLimitByDimensions{true};
  // spreadsheet gridlines
  TableGridlines tableGridlines{TableGridlines::SOFT};
};

void translate(Document document, const std::string &path, const Config &config);
void edit(Document document, const std::string &diff);

}

}

#endif // ODR_HTML_H
