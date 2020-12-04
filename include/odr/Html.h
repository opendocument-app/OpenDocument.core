#ifndef ODR_HTML_H
#define ODR_HTML_H

#include <cstdint>
#include <string>

namespace odr {
class Document;

enum class HtmlTableGridlines {
  NONE,
  SOFT,
  HARD,
};

struct HtmlConfig {
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
  HtmlTableGridlines tableGridlines{HtmlTableGridlines::SOFT};
};

namespace Html {
HtmlConfig parseConfig(const std::string &path);

void translate(Document document, const std::string &path,
               const HtmlConfig &config);
void edit(Document document, const std::string &diff);
} // namespace Html

} // namespace odr

#endif // ODR_HTML_H
