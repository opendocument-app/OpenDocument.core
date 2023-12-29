#ifndef ODR_INTERNAL_HTML_FILESYSTEM_H
#define ODR_INTERNAL_HTML_FILESYSTEM_H

#include <string>

namespace odr {
enum class FileType;
struct HtmlConfig;
class Html;
class Filesystem;
} // namespace odr

namespace odr::internal::html {

Html translate_filesystem(FileType file_type, const Filesystem &filesystem,
                          const std::string &output_path,
                          const HtmlConfig &config);

}

#endif // ODR_INTERNAL_HTML_FILESYSTEM_H
