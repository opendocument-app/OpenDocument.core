#include <odr/internal/html/common.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>

#include <fstream>
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

HtmlResourceLocator html::local_resource_locator(const std::string &output_path,
                                                 const HtmlConfig &config) {
  return [&](HtmlResourceType type, const std::string &name,
             const std::string &path, const File &resource,
             bool is_core_resource) -> HtmlResourceLocation {
    if (config.embed_resources) {
      return std::nullopt;
    }

    if (is_core_resource && !config.external_resource_path.empty()) {
      auto resource_path =
          common::Path(config.external_resource_path).join(path);
      if (config.relative_resource_paths) {
        resource_path = resource_path.rebase(output_path);
      }
      return resource_path.string();
    }

    // TODO relocate file if necessary

    auto resource_path = common::Path(output_path).join(path);
    std::filesystem::create_directories(resource_path.parent().path());
    std::ofstream os(resource_path.path());
    util::stream::pipe(*resource.stream(), os);
    return path;
  };
}

} // namespace odr::internal
