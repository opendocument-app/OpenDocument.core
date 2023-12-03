#include <odr/internal/html/common.hpp>

#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>

#include <iomanip>
#include <sstream>

namespace odr::internal {

const char *html::doctype() noexcept {
  // clang-format off
  return R"V0G0N(<!DOCTYPE html>
)V0G0N";
  // clang-format on
}

const char *html::default_headers() noexcept {
  // clang-format off
  return R"V0G0N(<meta charset="UTF-8"/>
<base target="_blank"/>
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=yes"/>
<title>odr</title>)V0G0N";
  // clang-format on
}

std::string html::body_attributes(const HtmlConfig &config) noexcept {
  std::string result;

  result += "class=\"";
  switch (config.spreadsheet_gridlines) {
  case HtmlTableGridlines::soft:
    result += "odr-gridlines-soft";
    break;
  case HtmlTableGridlines::hard:
    result += "odr-gridlines-hard";
    break;
  case HtmlTableGridlines::none:
  default:
    result += "odr-gridlines-none";
    break;
  }
  result += "\"";

  return result;
}

std::string html::escape_text(std::string text) noexcept {
  if (text.empty()) {
    return text;
  }

  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (text.front() == ' ') {
    text = "&nbsp;" + text.substr(1);
  }
  if (text.back() == ' ') {
    text = text.substr(0, text.length() - 1) + "&nbsp;";
  }
  util::string::replace_all(text, "  ", " &nbsp;");

  // TODO `&emsp;` is not a tab
  util::string::replace_all(text, "\t", "&emsp;");

  return text;
}

std::string html::color(const Color &color) noexcept {
  std::stringstream ss;
  ss << "#";
  ss << std::setw(6) << std::setfill('0') << std::hex << color.rgb();
  return ss.str();
}

} // namespace odr::internal
