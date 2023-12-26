#ifndef ODR_INTERNAL_HTML_IMAGE_FILE_H
#define ODR_INTERNAL_HTML_IMAGE_FILE_H

#include <string>

namespace odr {
class File;
class ImageFile;

struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::html {

void translate_image_src(const File &file, std::ostream &out,
                         const HtmlConfig &config);
void translate_image_src(const ImageFile &image_file, std::ostream &out,
                         const HtmlConfig &config);

Html translate_image_file(const ImageFile &image_file,
                          const std::string &output_path,
                          const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_IMAGE_FILE_H
