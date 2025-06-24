#pragma once

#include <memory>
#include <string>

namespace odr {
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal {
class WvWareLegacyMicrosoftFile;
} // namespace odr::internal

namespace odr::internal::html {

odr::HtmlService
create_wvware_oldms_service(const WvWareLegacyMicrosoftFile &oldms_file,
                            const std::string &cache_path, HtmlConfig config,
                            std::shared_ptr<Logger> logger);

}
