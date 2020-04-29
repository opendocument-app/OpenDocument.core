#ifndef ODR_CONFIG_H
#define ODR_CONFIG_H

#include <cstdint>

namespace odr {

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
  // javascript paging for odt
  bool paging{false};

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

} // namespace odr

#endif // ODR_CONFIG_H
