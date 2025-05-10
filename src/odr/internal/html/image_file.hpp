#pragma once

#include <string>

namespace odr {
class File;
class ImageFile;
struct HtmlConfig;
class HtmlService;
} // namespace odr

namespace odr::internal::html {

void translate_image_src(const File &file, std::ostream &out,
                         const HtmlConfig &config);
void translate_image_src(const ImageFile &image_file, std::ostream &out,
                         const HtmlConfig &config);

HtmlService create_image_service(const ImageFile &image_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

} // namespace odr::internal::html
