#include <odr/html_service.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <iostream>

namespace odr {

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

bool HtmlResource::is_relocatable() const { return m_impl->is_relocatable(); }

bool HtmlResource::is_external() const { return m_impl->is_external(); }

void HtmlResource::write_resource(std::ostream &os) const {
  m_impl->write_resource(os);
}

} // namespace odr
