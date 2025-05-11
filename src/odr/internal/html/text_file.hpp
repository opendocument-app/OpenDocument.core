#pragma once

#include <string>

namespace odr {
class TextFile;
struct HtmlConfig;
class HtmlService;
} // namespace odr

namespace odr::internal::html {

odr::HtmlService create_text_service(const TextFile &text_file,
                                     const std::string &cache_path,
                                     HtmlConfig config);

}
