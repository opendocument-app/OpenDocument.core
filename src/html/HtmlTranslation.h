#ifndef ODR_HTML_HTMLTRANSLATION_H
#define ODR_HTML_HTMLTRANSLATION_H

#include <string>

namespace odr {
struct Config;

namespace common {
class Document;
}

namespace html {

namespace HtmlTranslation {
void translate(const common::Document &document, const std::string &path, const Config &config);
void edit(const common::Document &document, const std::string &diff);
}

}

}

#endif // ODR_HTML_HTMLTRANSLATION_H
