#ifndef ODR_COMMON_CONSTANTS_H
#define ODR_COMMON_CONSTANTS_H

namespace odr {
namespace common {

namespace Constants {
const char *getVersion() noexcept;
const char *getCommit() noexcept;

const char *getHtmlBeginToStyle() noexcept;
const char *getHtmlStyleToBody() noexcept;
const char *getHtmlBodyToScript() noexcept;
const char *getHtmlScriptToEnd() noexcept;

const char *getOpenDocumentDefaultCss() noexcept;
const char *getOpenDocumentSpreadsheetDefaultCss() noexcept;

const char *getDefaultScript() noexcept;
} // namespace Constants

} // namespace common
} // namespace odr

#endif // ODR_COMMON_CONSTANTS_H
