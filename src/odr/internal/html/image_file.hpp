#pragma once

#include <memory>
#include <string>

namespace odr {
class File;
class ImageFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

void translate_image_src(const File &file, std::ostream &out,
                         const HtmlConfig &config, const Logger &logger);
void translate_image_src(const ImageFile &image_file, std::ostream &out,
                         const HtmlConfig &config, const Logger &logger);

HtmlService create_image_service(const ImageFile &image_file, HtmlConfig config,
                                 const Logger &logger);

} // namespace odr::internal::html
