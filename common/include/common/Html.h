#ifndef ODR_COMMON_HTML_H
#define ODR_COMMON_HTML_H

namespace odr {
namespace common {

namespace Html {
const char *getBeginToStyle() noexcept;
const char *getStyleToBody() noexcept;
const char *getBodyToScript() noexcept;
const char *getScriptToEnd() noexcept;

const char *getOpenDocumentDefaultCss() noexcept;
const char *getOpenDocumentSpreadsheetDefaultCss() noexcept;

const char *getDefaultScript() noexcept;
} // namespace Html

} // namespace common
} // namespace odr

#endif // ODR_COMMON_HTML_H
