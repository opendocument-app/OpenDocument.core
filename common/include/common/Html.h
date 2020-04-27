#ifndef ODR_COMMON_HTML_H
#define ODR_COMMON_HTML_H

#include <string>

namespace odr {
struct Config;

namespace access {
class Sink;
}

namespace common {

namespace Html {
const char *doctype() noexcept;

const char *defaultHeaders() noexcept;

const char *odfDefaultStyle() noexcept;
const char *odfSpreadsheetDefaultStyle() noexcept;

const char *defaultScript() noexcept;

std::string bodyAttributes(const Config &) noexcept;
} // namespace Html

} // namespace common
} // namespace odr

#endif // ODR_COMMON_HTML_H
