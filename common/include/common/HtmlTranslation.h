#ifndef ODR_COMMON_HTMLTRANSLATION_H
#define ODR_COMMON_HTMLTRANSLATION_H

#include <string>

namespace odr {
struct Config;

namespace common {
class GenericDocument;

namespace HtmlTranslation {
void translate(const GenericDocument &document, const std::string &path, const Config &config);
void edit(const GenericDocument &document, const std::string &diff);
}

}

}

#endif // ODR_COMMON_HTMLTRANSLATION_H
