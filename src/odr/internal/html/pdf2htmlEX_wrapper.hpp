#ifndef ODR_INTERNAL_PDF2HTMLEX_WRAPPER_HPP
#define ODR_INTERNAL_PDF2HTMLEX_WRAPPER_HPP

#include <string>

namespace odr {
class PdfFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html pdf2htmlEX_wrapper(const std::string &input_path, const std::string &output_path,
                        const HtmlConfig &config);

}

#endif // ODR_INTERNAL_PDF2HTMLEX_WRAPPER_HPP
