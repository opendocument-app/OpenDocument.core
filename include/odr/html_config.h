#ifndef ODR_HTML_CONFIG_H
#define ODR_HTML_CONFIG_H

#include <cstdint>

namespace odr {

enum class HtmlTableGridlines {
  NONE,
  SOFT,
  HARD,
};

struct HtmlConfig {
  // starting sheet for spreadsheet, starting page for presentation, ignored for
  // text, ignored for graphics
  std::uint32_t entry_offset{0};
  // translate only N sheets / pages; zero means translate all
  std::uint32_t entry_count{0};
  // create output for each entry
  bool split_entries{false};
  // create editable output
  bool editable{false};

  // text document margin
  bool text_document_margin{false};

  // spreadsheet table limit
  std::uint32_t table_limit_rows{10000};
  std::uint32_t table_limit_cols{500};
  bool table_limit_by_dimensions{true};
  // spreadsheet gridlines
  HtmlTableGridlines table_gridlines{HtmlTableGridlines::SOFT};
};

} // namespace odr

#endif // ODR_HTML_CONFIG_H
