#include <odr/html_service.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <odr/exceptions.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace odr {

namespace {

void bring_offline(const HtmlResources &resources,
                   const std::string &output_path) {
  for (const auto &[resource, location] : resources) {
    if (!location.has_value()) {
      continue;
    }
    if (!resource.file()) {
      continue;
    }
    auto path = odr::internal::common::Path(output_path)
                    .join(odr::internal::common::Path(*location));

    std::filesystem::create_directories(path.parent());
    std::ofstream ostream(path.string(), std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    resource.write_resource(ostream);
  }
}

} // namespace

HtmlService::HtmlService() = default;

HtmlService::HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl)
    : m_impl{std::move(impl)} {}

const HtmlConfig &HtmlService::config() const { return m_impl->config(); }

const HtmlResourceLocator &HtmlService::resource_locator() const {
  return m_impl->resource_locator();
}

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

  for (const auto &view : views) {
    auto path = odr::internal::common::Path(output_path)
                    .join(odr::internal::common::Path(view.path()));

    std::filesystem::create_directories(path.parent());
    std::ofstream ostream(path.string(), std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }

    HtmlResources view_resources = view.write_html(ostream);
    resources.insert(resources.end(), view_resources.begin(),
                     view_resources.end());

    pages.emplace_back(view.name(), path.string());
  }

  {
    auto it = std::unique(resources.begin(), resources.end(),
                          [](const auto &lhs, const auto &rhs) {
                            return lhs.first.path() == rhs.first.path();
                          });
    resources.erase(it, resources.end());
  }

  odr::bring_offline(resources, output_path);

  return {config(), std::move(pages)};
}

const std::shared_ptr<internal::abstract::HtmlService> &
HtmlService::impl() const {
  return m_impl;
}

HtmlView::HtmlView() = default;

HtmlView::HtmlView(std::shared_ptr<internal::abstract::HtmlView> impl)
    : m_impl{std::move(impl)} {}

const std::string &HtmlView::name() const { return m_impl->name(); }

const std::string &HtmlView::path() const { return m_impl->path(); }

const HtmlConfig &HtmlView::config() const { return m_impl->config(); }

HtmlResources HtmlView::write_html(std::ostream &out) const {
  internal::html::HtmlWriter writer(out, config());
  return m_impl->write_html(writer);
}

Html HtmlView::bring_offline(const std::string &output_path) const {
  HtmlResources resources;

  auto path = odr::internal::common::Path(output_path)
                  .join(odr::internal::common::Path(this->path()));

  {
    std::filesystem::create_directories(path.parent());
    std::ofstream ostream(path.string(), std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }

    resources = write_html(ostream);
  }

  odr::bring_offline(resources, output_path);

  return {config(), {{name(), path.string()}}};
}

const std::shared_ptr<internal::abstract::HtmlView> &HtmlView::impl() const {
  return m_impl;
}

HtmlResource::HtmlResource() = default;

HtmlResource::HtmlResource(
    std::shared_ptr<internal::abstract::HtmlResource> impl)
    : m_impl{std::move(impl)} {}

HtmlResourceType HtmlResource::type() const { return m_impl->type(); }

const std::string &HtmlResource::mime_type() const {
  return m_impl->mime_type();
}

const std::string &HtmlResource::name() const { return m_impl->name(); }

const std::string &HtmlResource::path() const { return m_impl->path(); }

const File &HtmlResource::file() const { return m_impl->file(); }

bool HtmlResource::is_shipped() const { return m_impl->is_shipped(); }

bool HtmlResource::is_external() const { return m_impl->is_external(); }

bool HtmlResource::is_accessible() const { return m_impl->is_accessible(); }

void HtmlResource::write_resource(std::ostream &os) const {
  m_impl->write_resource(os);
}

} // namespace odr
