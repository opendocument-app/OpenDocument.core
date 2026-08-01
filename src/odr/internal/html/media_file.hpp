#pragma once

#include <string>

namespace odr {
class DecodedFile;
struct HtmlConfig;
class HtmlService;
class Logger;
} // namespace odr

namespace odr::internal::html {

/// A player page for an audio or video file. There is no wrapper type to take
/// here — nothing is decoded, so the plain @ref DecodedFile carries everything
/// the page needs: the bytes and the file type they are named by.
HtmlService create_media_service(const DecodedFile &media_file,
                                 const std::string &cache_path,
                                 HtmlConfig config, const Logger &logger);

} // namespace odr::internal::html
