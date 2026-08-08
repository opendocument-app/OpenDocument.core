#include <odr/html.hpp>

#include <odr/archive.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/global_params.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/document.hpp>
#include <odr/internal/html/filesystem.hpp>
#include <odr/internal/html/font_file.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/html/media_file.hpp>
#include <odr/internal/html/pdf_file.hpp>
#include <odr/internal/html/text_file.hpp>
#include <odr/internal/util/file_util.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

using namespace odr::internal;

namespace odr {

namespace {

void bring_offline(const HtmlResources &resources,
                   const std::string &output_path) {
  for (const auto &[resource, location] : resources) {
    if (!location.has_value() || resource.is_external() ||
        !resource.is_accessible()) {
      continue;
    }
    const Path path = Path(output_path).join(RelPath(*location));

    std::filesystem::create_directories(path.parent().path());
    std::ofstream ostream = util::file::create(path.string());
    resource.write_resource(ostream);
  }
}

} // namespace

HtmlConfig::HtmlConfig() { init(); }

HtmlConfig::HtmlConfig(std::string output_path_)
    : output_path(std::move(output_path_)) {
  init();
}

void HtmlConfig::init() {
  resource_path = GlobalParams::odr_core_data_path();
  resource_locator = html::standard_resource_locator();
}

Html::Html(HtmlConfig config, std::vector<HtmlPage> pages)
    : m_config{std::move(config)}, m_pages{std::move(pages)} {}

const HtmlConfig &Html::config() const { return m_config; }

const std::vector<HtmlPage> &Html::pages() const { return m_pages; }

HtmlPage::HtmlPage(std::string name, std::string path)
    : name{std::move(name)}, path{std::move(path)} {}

HtmlService::HtmlService() = default;

HtmlService::HtmlService(std::shared_ptr<abstract::HtmlService> impl)
    : m_impl{std::move(impl)} {}

const HtmlConfig &HtmlService::config() const { return m_impl->config(); }

const HtmlViews &HtmlService::list_views() const {
  return m_impl->list_views();
}

void HtmlService::warmup() const { m_impl->warmup(); }

bool HtmlService::exists(const std::string &path) const {
  return m_impl->exists(path);
}

std::string HtmlService::mimetype(const std::string &path) const {
  return m_impl->mimetype(path);
}

void HtmlService::write(const std::string &path, std::ostream &out) const {
  m_impl->write(path, out);
}

HtmlResources HtmlService::write_html(const std::string &path,
                                      std::ostream &out) const {
  internal::html::HtmlWriter writer(out, config());
  return m_impl->write_html(path, writer);
}

Html HtmlService::bring_offline(const std::string &output_path) const {
  return bring_offline(output_path, list_views());
}

Html HtmlService::bring_offline(const std::string &output_path,
                                const std::vector<HtmlView> &views) const {
  std::vector<HtmlPage> pages;

  HtmlResources resources;

  for (const HtmlView &view : views) {
    const Path path = Path(output_path).join(RelPath(view.path()));

    std::filesystem::create_directories(path.parent().path());
    std::ofstream ostream = util::file::create(path.string());
    HtmlResources view_resources = view.write_html(ostream);

    resources.insert(resources.end(), view_resources.begin(),
                     view_resources.end());

    pages.emplace_back(view.name(), path.string());
  }

  // a resource shared by two views appears once per view, and those need not be
  // adjacent - dedup by path, keeping the first occurrence and the order
  {
    std::unordered_set<std::string> seen;
    const auto removed =
        std::ranges::remove_if(resources, [&seen](const auto &resource) {
          return !seen.insert(resource.first.path()).second;
        });
    resources.erase(removed.begin(), removed.end());
  }

  odr::bring_offline(resources, output_path);

  return {config(), std::move(pages)};
}

const std::shared_ptr<abstract::HtmlService> &HtmlService::impl() const {
  return m_impl;
}

HtmlView::HtmlView() = default;

HtmlView::HtmlView(std::shared_ptr<abstract::HtmlView> impl)
    : m_impl{std::move(impl)} {}

const std::string &HtmlView::name() const { return m_impl->name(); }

std::size_t HtmlView::index() const { return m_impl->index(); }

const std::string &HtmlView::path() const { return m_impl->path(); }

const HtmlConfig &HtmlView::config() const { return m_impl->config(); }

HtmlResources HtmlView::write_html(std::ostream &out) const {
  internal::html::HtmlWriter writer(out, config());
  return m_impl->write_html(writer);
}

Html HtmlView::bring_offline(const std::string &output_path) const {
  HtmlResources resources;

  const Path path = Path(output_path).join(RelPath(this->path()));

  {
    std::filesystem::create_directories(path.parent().path());
    std::ofstream ostream = util::file::create(path.string());
    resources = write_html(ostream);
  }

  odr::bring_offline(resources, output_path);

  return {config(), {{name(), path.string()}}};
}

const std::shared_ptr<abstract::HtmlView> &HtmlView::impl() const {
  return m_impl;
}

HtmlResource::HtmlResource() = default;

HtmlResource::HtmlResource(std::shared_ptr<abstract::HtmlResource> impl)
    : m_impl{std::move(impl)} {}

HtmlResourceType HtmlResource::type() const { return m_impl->type(); }

const std::string &HtmlResource::mime_type() const {
  return m_impl->mime_type();
}

const std::string &HtmlResource::name() const { return m_impl->name(); }

const std::string &HtmlResource::path() const { return m_impl->path(); }

const std::optional<File> &HtmlResource::file() const { return m_impl->file(); }

bool HtmlResource::is_shipped() const { return m_impl->is_shipped(); }

bool HtmlResource::is_external() const { return m_impl->is_external(); }

bool HtmlResource::is_accessible() const { return m_impl->is_accessible(); }

void HtmlResource::write_resource(std::ostream &os) const {
  m_impl->write_resource(os);
}

HtmlService html::translate(const DecodedFile &file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  if (file.is_text_file()) {
    return translate(file.as_text_file(), cache_path, config, logger);
  }
  if (file.is_image_file()) {
    return translate(file.as_image_file(), cache_path, config, logger);
  }
  if (file.is_archive_file()) {
    return translate(file.as_archive_file(), cache_path, config, logger);
  }
  if (file.is_document_file()) {
    return translate(file.as_document_file(), cache_path, config, logger);
  }
  if (file.is_pdf_file()) {
    return translate(file.as_pdf_file(), cache_path, config, logger);
  }
  if (file.is_font_file()) {
    return translate(file.as_font_file(), cache_path, config, logger);
  }
  // No wrapper type to go through: nothing is decoded, so the plain
  // `DecodedFile` already carries the bytes and the type that names them.
  if (const FileCategory category = file.file_category();
      category == FileCategory::audio || category == FileCategory::video) {
    std::filesystem::create_directories(cache_path);
    return internal::html::create_media_service(file, cache_path, config,
                                                logger);
  }

  throw UnsupportedFileType(file.file_type());
}

HtmlResourceLocator html::standard_resource_locator() {
  return [](const HtmlResource &resource,
            const HtmlConfig &config) -> HtmlResourceLocation {
    // only an accessible image can be embedded; everything else is linked
    if (resource.is_accessible() && config.embed_images &&
        resource.type() == HtmlResourceType::image) {
      return std::nullopt;
    }

    return resource.path();
  };
}

HtmlService html::translate(const TextFile &text_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  std::filesystem::create_directories(cache_path);
  return internal::html::create_text_service(text_file, cache_path, config,
                                             logger);
}

HtmlService html::translate(const ImageFile &image_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  std::filesystem::create_directories(cache_path);
  return internal::html::create_image_service(image_file, cache_path, config,
                                              logger);
}

HtmlService html::translate(const ArchiveFile &archive_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  return translate(archive_file.archive(), cache_path, config, logger);
}

HtmlService html::translate(const DocumentFile &document_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  return translate(document_file.document(), cache_path, config, logger);
}

HtmlService html::translate(const PdfFile &pdf_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  return internal::html::create_pdf_service(pdf_file, cache_path, config,
                                            logger);
}

HtmlService html::translate(const FontFile &font_file,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  std::filesystem::create_directories(cache_path);
  return internal::html::create_font_service(font_file, cache_path, config,
                                             logger);
}

HtmlService html::translate(const Filesystem &filesystem,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  std::filesystem::create_directories(cache_path);
  return internal::html::create_filesystem_service(filesystem, cache_path,
                                                   config, logger);
}

HtmlService html::translate(const Archive &archive,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  return translate(archive.as_filesystem(), cache_path, config, logger);
}

HtmlService html::translate(const Document &document,
                            const std::string &cache_path,
                            const HtmlConfig &config, const Logger &logger) {
  std::filesystem::create_directories(cache_path);
  return internal::html::create_document_service(document, cache_path, config,
                                                 logger);
}

void html::edit(const Document &document, const std::string_view diff,
                const Logger & /*logger*/) {
  const nlohmann::json json = nlohmann::json::parse(diff);
  for (const auto &[key, value] : json["modifiedText"].items()) {
    const Element element =
        document.root_element().navigate_path(DocumentPath(key));
    if (!element) {
      throw std::invalid_argument("element with path " + key + " not found");
    }
    if (!element.as_text()) {
      throw std::invalid_argument("element with path " + key +
                                  " is not a text element");
    }
    element.as_text().set_content(value);
  }
}

} // namespace odr
