#ifndef ODR_INTERNAL_COMMON_HTML_H
#define ODR_INTERNAL_COMMON_HTML_H

#include <string>

namespace odr {
struct HtmlConfig;
}

namespace odr::internal::common::html {
const char *doctype() noexcept;

const char *default_headers() noexcept;

const char *default_style() noexcept;
const char *default_spreadsheet_style() noexcept;

const char *default_script() noexcept;

std::string body_attributes(const HtmlConfig &config) noexcept;

std::string escape_text(std::string text) noexcept;
} // namespace odr::internal::common::html

#endif // ODR_INTERNAL_COMMON_HTML_H
