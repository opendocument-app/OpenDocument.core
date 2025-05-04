#include <odr/internal/html/html_service.hpp>

#include <odr/internal/util/stream_util.hpp>

#include <iostream>
#include <utility>

namespace odr::internal::html {

HtmlService::HtmlService(HtmlConfig config,
                         HtmlResourceLocator resource_locator)
    : m_config{std::move(config)},
      m_resource_locator{std::move(resource_locator)} {}

const HtmlConfig &HtmlService::config() const { return m_config; }

const HtmlResourceLocator &HtmlService::resource_locator() const {
  return m_resource_locator;
}

odr::HtmlResource HtmlResource::create(HtmlResourceType type,
                                       std::string mime_type, std::string name,
                                       std::string path, odr::File file,
                                       bool is_shipped, bool is_external,
                                       bool is_accessible) {
  return odr::HtmlResource(std::make_shared<HtmlResource>(
      type, std::move(mime_type), std::move(name), std::move(path),
      std::move(file), is_shipped, is_external, is_accessible));
}

HtmlResource::HtmlResource(HtmlResourceType type, std::string mime_type,
                           std::string name, std::string path, odr::File file,
                           bool is_shipped, bool is_external,
                           bool is_accessible)
    : m_type{type}, m_mime_type{std::move(mime_type)}, m_name{std::move(name)},
      m_path{std::move(path)}, m_file{std::move(file)},
      m_is_shipped{is_shipped}, m_is_external{is_external},
      m_is_accessible{is_accessible} {}

HtmlResourceType HtmlResource::type() const { return m_type; }

const std::string &HtmlResource::mime_type() const { return m_mime_type; }

const std::string &HtmlResource::name() const { return m_name; }

const std::string &HtmlResource::path() const { return m_path; }

const odr::File &HtmlResource::file() const { return m_file; }

bool HtmlResource::is_shipped() const { return m_is_shipped; }

bool HtmlResource::is_external() const { return m_is_external; }

bool HtmlResource::is_accessible() const { return m_is_accessible; }

void HtmlResource::write_resource(std::ostream &os) const {
  util::stream::pipe(*m_file.stream(), os);
}

} // namespace odr::internal::html
