#include <odr/html_service.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <iostream>

namespace odr {

HtmlDocumentService::HtmlDocumentService() = default;

HtmlDocumentService::HtmlDocumentService(
    std::shared_ptr<internal::abstract::HtmlDocumentService> impl)
    : m_impl{std::move(impl)} {}

const HtmlConfig &HtmlDocumentService::config() const {
  return m_impl->config();
}

const HtmlResourceLocator &HtmlDocumentService::resource_locator() const {
  return m_impl->resource_locator();
}

HtmlResources HtmlDocumentService::write_document(std::ostream &os) const {
  internal::html::HtmlWriter out(os, config());

  return m_impl->write_document(out);
}

const std::shared_ptr<internal::abstract::HtmlDocumentService> &
HtmlDocumentService::impl() const {
  return m_impl;
}

HtmlFragmentService::HtmlFragmentService(
    std::shared_ptr<internal::abstract::HtmlFragmentService> impl)
    : m_impl{std::move(impl)} {}

const HtmlConfig &HtmlFragmentService::config() const {
  return m_impl->config();
}

const HtmlResourceLocator &HtmlFragmentService::resource_locator() const {
  return m_impl->resource_locator();
}

void HtmlFragmentService::write_fragment(std::ostream &os,
                                         HtmlResources &resources) const {
  internal::html::HtmlWriter out(os, config());

  m_impl->write_fragment(out, resources);
}

const std::shared_ptr<internal::abstract::HtmlFragmentService> &
HtmlFragmentService::impl() const {
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
