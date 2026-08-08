#pragma once

#include <memory>
#include <string>

namespace odr {
struct HtmlConfig;
class HtmlService;
class Filesystem;
class Logger;
} // namespace odr

namespace odr::internal::html {

HtmlService create_filesystem_service(const Filesystem &filesystem,
                                      HtmlConfig config, const Logger &logger);

}
