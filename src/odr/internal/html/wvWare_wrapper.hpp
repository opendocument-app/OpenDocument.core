#ifndef ODR_INTERNAL_WVWARE_WRAPPER_HPP
#define ODR_INTERNAL_WVWARE_WRAPPER_HPP

#include <string>

namespace odr {
class File;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

Html wvWare_wrapper(const std::string &input_path,
                    const std::string &output_path, const HtmlConfig &config,
                    std::optional<std::string> &password);

}

#endif // ODR_INTERNAL_WVWARE_WRAPPER_HPP
