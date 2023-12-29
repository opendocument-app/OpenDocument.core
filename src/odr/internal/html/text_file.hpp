#ifndef ODR_INTERNAL_HTML_TEXT_FILE_H
#define ODR_INTERNAL_HTML_TEXT_FILE_H

#include <string>

namespace odr {
class TextFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html translate_text_file(const TextFile &text_file,
                         const std::string &output_path,
                         const HtmlConfig &config);

}

#endif // ODR_INTERNAL_HTML_TEXT_FILE_H
