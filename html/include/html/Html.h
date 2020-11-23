#ifndef ODR_HTML_HTML_H
#define ODR_HTML_HTML_H

#include <string>

namespace odr {
struct Config;

namespace html::Html {
const char *doctype() noexcept;

const char *defaultHeaders() noexcept;

const char *odfDefaultStyle() noexcept;
const char *odfSpreadsheetDefaultStyle() noexcept;

const char *defaultScript() noexcept;

std::string bodyAttributes(const Config &) noexcept;

std::string escapeText(std::string text) noexcept;
} // namespace common::Html
} // namespace odr

#endif // ODR_HTML_HTML_H
