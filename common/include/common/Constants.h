#ifndef ODR_TEMPLATES_H
#define ODR_TEMPLATES_H

namespace odr {

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

} // namespace odr

#endif // ODR_TEMPLATES_H
