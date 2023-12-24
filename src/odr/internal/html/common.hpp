#ifndef ODR_INTERNAL_HTML_COMMON_H
#define ODR_INTERNAL_HTML_COMMON_H

#include <string>

namespace odr {
struct Color;
struct HtmlConfig;
} // namespace odr

namespace odr::internal::html {

std::string escape_text(std::string text) noexcept;

std::string color(const Color &color) noexcept;

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_COMMON_H
