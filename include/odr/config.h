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
  std::uint32_t entry_offset{0};
  // translate only N sheets / pages; zero means translate all
  std::uint32_t entry_count{0};
  // create output for each entry
  bool split_entries{false};
  // create editable output
  bool editable{false};

  // spreadsheet table limit
  std::uint32_t table_limit_rows{10000};
  std::uint32_t table_limit_cols{500};
  bool tableLimit_by_dimensions{true};
  // spreadsheet gridlines
  TableGridlines table_gridlines{TableGridlines::SOFT};
};

} // namespace odr

#endif // ODR_CONFIG_H
