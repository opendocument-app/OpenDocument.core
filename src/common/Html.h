#ifndef ODR_COMMON_HTML_H
#define ODR_COMMON_HTML_H

#include <string>

namespace odr {
struct HtmlConfig;

namespace common::Html {
const char *doctype() noexcept;

const char *defaultHeaders() noexcept;

const char *defaultStyle() noexcept;
const char *defaultSpreadsheetStyle() noexcept;

const char *defaultScript() noexcept;

std::string bodyAttributes(const HtmlConfig &) noexcept;

std::string escapeText(std::string text) noexcept;
} // namespace common::Html
} // namespace odr

#endif // ODR_COMMON_HTML_H
