#pragma once

#include <string>

namespace odr {
struct HtmlConfig;
class HtmlService;
} // namespace odr

namespace odr::internal {
class WvWareLegacyMicrosoftFile;
} // namespace odr::internal

namespace odr::internal::html {

HtmlService
create_wvware_oldms_service(const WvWareLegacyMicrosoftFile &oldms_file,
                            const std::string &output_path,
                            const HtmlConfig &config);

}
