#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
#include <sstream>

namespace odr::internal {

Html html::translate_text_file(const TextFile &text_file,
                               const std::string &path,
                               const HtmlConfig &config) {}

} // namespace odr::internal
