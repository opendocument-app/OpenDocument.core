#include <odr/internal/html/common.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/html.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

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
                              const std::string &mime_type) {
  return "data:" + mime_type + ";base64," + crypto::util::base64_encode(file);
}

std::string html::file_to_url(std::istream &file,
                              const std::string &mime_type) {
  return file_to_url(util::stream::read(file), mime_type);
}

std::string html::file_to_url(const abstract::File &file,
                              const std::string &mime_type) {
  return file_to_url(*file.stream(), mime_type);
}

HtmlResourceLocator html::local_resource_locator(std::string output_path,
                                                 HtmlConfig config) {
  return [output_path = std::move(output_path), config = std::move(config)](
             const odr::HtmlResource &resource) -> HtmlResourceLocation {
    if (!resource.is_accessible()) {
      return resource.path();
    }

    if ((config.embed_shipped_resources && resource.is_shipped()) ||
        (config.embed_images && resource.type() == HtmlResourceType::image)) {
      return std::nullopt;
    }

    if (resource.is_shipped()) {
      auto resource_path = common::Path(config.resource_path)
                               .join(common::Path(resource.path()));
      if (config.relative_resource_paths) {
        resource_path = resource_path.rebase(common::Path(output_path));
      }
      return resource_path.string();
    }

    auto resource_path =
        common::Path(output_path).join(common::Path(resource.path()));
    std::filesystem::create_directories(resource_path.parent().path());
    std::ofstream os = util::file::create(resource_path.path());
    resource.write_resource(os);
    return resource.path();
  };
}

} // namespace odr::internal
