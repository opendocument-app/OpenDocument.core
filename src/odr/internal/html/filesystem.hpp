#pragma once

#include <memory>
#include <string>

namespace odr {
enum class FileType;
struct HtmlConfig;
class HtmlService;
class Filesystem;
class Logger;
} // namespace odr

namespace odr::internal::html {

HtmlService create_filesystem_service(const Filesystem &filesystem,
                                      const std::string &cache_path,
                                      HtmlConfig config,
                                      std::shared_ptr<Logger> logger);

}
