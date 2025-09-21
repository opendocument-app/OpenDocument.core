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
                         const HtmlConfig &config);
void translate_image_src(const ImageFile &image_file, std::ostream &out,
                         const HtmlConfig &config);

HtmlService create_image_service(const ImageFile &image_file,
                                 const std::string &cache_path,
                                 HtmlConfig config,
                                 std::shared_ptr<Logger> logger);

} // namespace odr::internal::html
