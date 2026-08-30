#include <odr/internal/html/media_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace odr::internal::html {
namespace {

/// The extension the media goes out under: the file type's canonical one,
/// unless the file came from disk named with another extension the same type
/// claims. `.mkv` and `.webm` are one type here - the two are the same bytes
/// down to the EBML DocType, deeper than a signature reaches - and a browser
/// handed a webm under the matroska name and MIME will not play it.
std::string source_extension(const DecodedFile &media_file) {
  const std::span<const std::string_view> extensions =
      file_extensions_by_file_type(media_file.file_type());
  if (extensions.empty()) {
    return "bin";
  }

  if (const std::optional<std::string> path = media_file.file().disk_path();
      path.has_value()) {
    std::string extension = std::filesystem::path(*path).extension().string();
    if (!extension.empty()) {
      extension.erase(0, 1); // the dot
      extension = util::string::to_lower(extension);
      if (std::ranges::find(extensions, extension) != extensions.end()) {
        return extension;
      }
    }
  }

  return std::string(extensions.front());
}

/// The MIME type to serve @p extension as: the file type's canonical one,
/// unless @p extension is not the canonical extension either - then the alias
/// whose subtype names it, so a `.webm` goes out as `video/webm` rather than
/// `video/x-matroska`.
std::string mime_type_for(const FileType file_type,
                          const std::string_view extension) {
  const std::span<const std::string_view> mimetypes =
      mimetypes_by_file_type(file_type);
  if (mimetypes.empty()) {
    return "application/octet-stream";
  }

  const std::span<const std::string_view> extensions =
      file_extensions_by_file_type(file_type);
  if (extensions.empty() || extension == extensions.front()) {
    return std::string(mimetypes.front());
  }

  for (const std::string_view mimetype : mimetypes) {
    std::string_view subtype = mimetype.substr(mimetype.find('/') + 1);
    if (subtype.starts_with("x-")) {
      subtype.remove_prefix(2);
    }
    if (subtype == extension) {
      return std::string(mimetype);
    }
  }

  return std::string(mimetypes.front());
}

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(DecodedFile media_file, HtmlConfig config,
                  const Logger &logger)
      : HtmlService(std::move(config), logger),
        m_media_file{std::move(media_file)},
        m_element{m_media_file.file_category() == FileCategory::video
                      ? "video"
                      : "audio"},
        m_view_path{m_element + ".html"},
        m_extension{source_extension(m_media_file)},
        m_source_path{m_element + "." + m_extension},
        m_mime_type{mime_type_for(m_media_file.file_type(), m_extension)},
        m_resources{locate_media_resources(this->config())} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, m_element, 0, m_view_path));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == m_view_path || path == m_source_path ||
           resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == m_view_path) {
      return "text/html";
    }
    if (path == m_source_path) {
      return m_mime_type;
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == m_view_path) {
      HtmlWriter writer(out, config());
      write_media(writer);
      return;
    }
    if (path == m_source_path) {
      m_media_file.file().pipe(out);
      return;
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      resource->write_resource(out);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == m_view_path) {
      return write_media(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_media(HtmlWriter &out) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources, logger());

    // The media stays a resource rather than a data URI: a video is regularly
    // larger than everything else we emit put together, and base64 in the
    // markup would cost a third on top of it again.
    const odr::HtmlResource resource = HtmlResource::create(
        HtmlResourceType::media, m_mime_type, m_source_path, m_source_path,
        m_media_file.file(), false, false, true);
    const HtmlResourceLocation location =
        config().resource_locator(resource, config());
    resources.emplace_back(resource, location);

    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);
    write_media_style(state);
    out.write_header_end();

    out.write_body_begin(HtmlElementOptions().set_class("odr-media"));

    out.write_element_begin(
        m_element,
        HtmlElementOptions()
            .set_extra(m_element == "video"
                           ? "controls playsinline preload=\"metadata\""
                           : "controls preload=\"metadata\"")
            .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
              if (location.has_value()) {
                clb("src", xml::escape_attribute(*location));
              } else {
                clb("src", [&](std::ostream &o) {
                  o << file_to_url(*m_media_file.file().impl(), m_mime_type);
                });
              }
            }));
    out.out() << "Error: " << m_element << " not supported by this browser";
    out.write_element_end(m_element);

    out.write_body_end();
    out.write_end();

    return resources;
  }

private:
  DecodedFile m_media_file;

  std::string m_element;
  std::string m_view_path;
  std::string m_extension;
  std::string m_source_path;
  std::string m_mime_type;
  /// The css this view links; empty of locations when the config embeds it.
  HtmlResources m_resources;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_media_service(const DecodedFile &media_file,
                                       HtmlConfig config,
                                       const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(media_file, std::move(config), logger));
}

} // namespace odr::internal
