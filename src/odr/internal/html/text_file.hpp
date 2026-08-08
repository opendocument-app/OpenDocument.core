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

HtmlService create_text_service(const TextFile &text_file, HtmlConfig config,
                                const Logger &logger);

}
