#pragma once

namespace odr {
class TextFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

/// Renders @p text_file as an indented, highlighted, foldable source view.
/// @throws NoXmlFile if @p text_file was not decoded as a
///         @ref odr::FileType::xml.
HtmlService create_xml_service(const TextFile &text_file, HtmlConfig config,
                               const Logger &logger);

} // namespace odr::internal::html
