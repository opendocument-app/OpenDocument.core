#pragma once

#include <memory>

namespace odr {
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::svg {
class SvgFile;
} // namespace odr::internal::svg

namespace odr::internal::html {

/// Renders @p svg_file by writing its markup into the page rather than as a
/// data url.
HtmlService create_svg_service(const std::shared_ptr<svg::SvgFile> &svg_file,
                               HtmlConfig config, const Logger &logger);

} // namespace odr::internal::html
