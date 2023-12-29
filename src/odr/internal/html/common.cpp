#include <odr/internal/html/common.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace odr::internal {

std::string html::escape_text(std::string text) {
  if (text.empty()) {
    return text;
  }

  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (text.front() == ' ') {
    text = "&nbsp;" + text.substr(1);
  }
  if (text.back() == ' ') {
    text = text.substr(0, text.length() - 1) + "&nbsp;";
  }
  util::string::replace_all(text, "  ", " &nbsp;");

  // TODO `&emsp;` is not a tab
  util::string::replace_all(text, "\t", "&emsp;");

  return text;
}

std::string html::color(const Color &color) {
  std::stringstream ss;
  ss << "#";
  ss << std::setw(6) << std::setfill('0') << std::hex << color.rgb();
  return ss.str();
}

std::string html::file_to_url(const std::string &file,
                              const std::string &mimeType) {
  return "data:" + mimeType + ";base64," + crypto::util::base64_encode(file);
}

std::string html::file_to_url(std::istream &file, const std::string &mimeType) {
  return file_to_url(util::stream::read(file), mimeType);
}

std::string html::file_to_url(const abstract::File &file,
                              const std::string &mimeType) {
  return file_to_url(*file.stream(), mimeType);
}

} // namespace odr::internal
