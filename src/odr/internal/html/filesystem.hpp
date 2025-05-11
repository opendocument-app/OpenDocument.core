#pragma once

#include <string>

namespace odr {
enum class FileType;
struct HtmlConfig;
class HtmlService;
class Filesystem;
} // namespace odr

namespace odr::internal::html {

odr::HtmlService create_filesystem_service(const Filesystem &filesystem,
                                           const std::string &cache_path,
                                           HtmlConfig config);

}
