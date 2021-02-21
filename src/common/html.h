#ifndef ODR_COMMON_HTML_H
#define ODR_COMMON_HTML_H

#include <string>

namespace odr {
struct HtmlConfig;

namespace common::Html {
const char *doctype() noexcept;

const char *default_headers() noexcept;

const char *default_style() noexcept;
const char *default_spreadsheet_style() noexcept;

const char *default_script() noexcept;

std::string body_attributes(const HtmlConfig &config) noexcept;

std::string escape_text(std::string text) noexcept;
} // namespace common::Html
} // namespace odr

#endif // ODR_COMMON_HTML_H
