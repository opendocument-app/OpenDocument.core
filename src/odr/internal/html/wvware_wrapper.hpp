#ifndef ODR_INTERNAL_WVWARE_WRAPPER_HPP
#define ODR_INTERNAL_WVWARE_WRAPPER_HPP

#include <string>

namespace odr {
struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal {
class WvWareLegacyMicrosoftFile;
} // namespace odr::internal

namespace odr::internal::html {

Html translate_wvware_oldms_file(const WvWareLegacyMicrosoftFile &oldms_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

}

#endif // ODR_INTERNAL_WVWARE_WRAPPER_HPP
