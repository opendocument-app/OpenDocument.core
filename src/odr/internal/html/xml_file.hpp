#pragma once

namespace odr {
class TextFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

/// Renders @p text_file, which has to be a @ref odr::FileType::xml, as an
/// indented, highlighted, foldable source view.
HtmlService create_xml_service(const TextFile &text_file, HtmlConfig config,
                               const Logger &logger);

} // namespace odr::internal::html
