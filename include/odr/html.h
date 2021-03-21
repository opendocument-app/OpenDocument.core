#ifndef ODR_HTML_H
#define ODR_HTML_H

#include <string>

namespace odr {
struct HtmlConfig;
class Document;

namespace Html {
HtmlConfig parse_config(const std::string &path);

void translate(const Document &document, const std::string &document_identifier,
               const std::string &path, const HtmlConfig &config);
void edit(const Document &document, const std::string &document_identifier,
          const std::string &diff);
} // namespace Html

} // namespace odr

#endif // ODR_HTML_H
