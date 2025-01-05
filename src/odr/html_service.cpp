#include <odr/html_service.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <iostream>

namespace odr {

HtmlService::HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl)
    : m_impl{std::move(impl)} {}

const HtmlConfig &HtmlService::config() const { return m_impl->config(); }

const HtmlResourceLocator &HtmlService::resource_locator() const {
  return m_impl->resource_locator();
}

std::vector<HtmlFragment> HtmlService::fragments() const {
  std::vector<HtmlFragment> result;
  for (const auto &fragment : m_impl->fragments()) {
    result.emplace_back(fragment);
  }
  return result;
}

HtmlResources HtmlService::write_document(std::ostream &os) const {
  internal::html::HtmlWriter out(os, config());

  auto internal_resources = m_impl->write_document(out);

  HtmlResources resources;
  for (const auto &[resource, location] : internal_resources) {
    resources.emplace_back(HtmlResource(resource), location);
  }
  return resources;
}

HtmlFragment::HtmlFragment(
    std::shared_ptr<internal::abstract::HtmlFragment> impl)
    : m_impl{std::move(impl)} {}

std::string HtmlFragment::name() const { return m_impl->name(); }

const HtmlConfig &HtmlFragment::config() const { return m_impl->config(); }

const HtmlResourceLocator &HtmlFragment::resource_locator() const {
  return m_impl->resource_locator();
}

void HtmlFragment::write_fragment(std::ostream &os,
                                  HtmlResources &resources) const {
  internal::html::HtmlWriter out(os, config());

  m_impl->write_fragment(out, resources);
}

HtmlResources HtmlFragment::write_document(std::ostream &os) const {
  internal::html::HtmlWriter out(os, config());

  auto internal_resources = m_impl->write_document(out);

  HtmlResources resources;
  for (const auto &[resource, location] : internal_resources) {
    resources.emplace_back(HtmlResource(resource), location);
  }
  return resources;
}

HtmlResource::HtmlResource() = default;

HtmlResource::HtmlResource(
    std::shared_ptr<internal::abstract::HtmlResource> impl)
    : m_impl{std::move(impl)} {}

HtmlResourceType HtmlResource::type() const { return m_impl->type(); }

const std::string &HtmlResource::name() const { return m_impl->name(); }

const std::string &HtmlResource::path() const { return m_impl->path(); }

const File &HtmlResource::file() const { return m_impl->file(); }

bool HtmlResource::is_shipped() const { return m_impl->is_shipped(); }

bool HtmlResource::is_relocatable() const { return m_impl->is_relocatable(); }

void HtmlResource::write_resource(std::ostream &os) const {
  m_impl->write_resource(os);
}

} // namespace odr
