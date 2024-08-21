#ifndef ODR_INTERNAL_WVWARE_WRAPPER_HPP
#define ODR_INTERNAL_WVWARE_WRAPPER_HPP

#include <string>

namespace odr {
class File;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html wvWare_wrapper(const File &file, const std::string &output_path,
                    const HtmlConfig &config);

}

#endif // ODR_INTERNAL_WVWARE_WRAPPER_HPP
