#pragma once

#include <memory>
#include <string>

namespace odr {
class TextFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

HtmlService create_text_service(const TextFile &text_file,
                                const std::string &cache_path,
                                HtmlConfig config,
                                std::shared_ptr<Logger> logger);

}
