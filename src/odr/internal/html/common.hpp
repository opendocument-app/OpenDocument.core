#ifndef ODR_INTERNAL_HTML_COMMON_H
#define ODR_INTERNAL_HTML_COMMON_H

#include <iosfwd>
#include <string>

namespace odr {
struct Color;
struct HtmlConfig;
} // namespace odr

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::html {

std::string escape_text(std::string text);

std::string color(const Color &color);

std::string file_to_url(const std::string &file, const std::string &mimeType);
std::string file_to_url(std::istream &file, const std::string &mimeType);
std::string file_to_url(const abstract::File &file,
                        const std::string &mimeType);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_COMMON_H
