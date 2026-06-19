#pragma once

#include <memory>
#include <string>

namespace odr {
class FontFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

/// @brief A specimen-page HTML service for a standalone font file: a
/// name/metrics header plus a glyph grid showing *every* glyph (including ones
/// the original `cmap` never reached), the font served via `@font-face` after
/// the uniform PUA re-encode.
HtmlService create_font_service(const FontFile &font_file,
                                const std::string &cache_path,
                                HtmlConfig config,
                                std::shared_ptr<Logger> logger);

} // namespace odr::internal::html
