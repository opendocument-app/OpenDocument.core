#pragma once

#include <memory>
#include <string>

namespace odr {
class File;
class ImageFile;
class HtmlResource;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

void translate_image_src(const File &file, std::ostream &out,
                         const HtmlConfig &config, const Logger &logger);
void translate_image_src(const ImageFile &image_file, std::ostream &out,
                         const HtmlConfig &config, const Logger &logger);

/// The image @p file as a file of its own at @p path, holding what its data
/// url would: an SVM converted to SVG, with `.svg` after the path, and
/// anything else as it is, with its media type.
odr::HtmlResource linked_image_resource(const File &file, std::string path,
                                        const Logger &logger);

HtmlService create_image_service(const ImageFile &image_file, HtmlConfig config,
                                 const Logger &logger);

} // namespace odr::internal::html
