#ifndef ODR_CONSTANTS_H
#define ODR_CONSTANTS_H

namespace odr {
namespace common {

namespace Constants {
extern const char *getVersion();
extern const char *getCommit();

extern const char *getHtmlBeginToStyle();
extern const char *getHtmlStyleToBody();
extern const char *getHtmlBodyToScript();
extern const char *getHtmlScriptToEnd();

extern const char *getOpenDocumentDefaultCss();
extern const char *getOpenDocumentSpreadsheetDefaultCss();

extern const char *getDefaultScript();
} // namespace Constants

} // namespace common
} // namespace odr

#endif // ODR_CONSTANTS_H
