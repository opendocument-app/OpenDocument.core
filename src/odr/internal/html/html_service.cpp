#include "odr/internal/html/html_service.hpp"

#include "odr/internal/util/stream_util.hpp"

#include <iostream>

namespace odr::internal::common {

HtmlService::HtmlService(
    HtmlConfig config, HtmlResourceLocator resource_locator,
    std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments)
    : m_config{config}, m_resource_locator{resource_locator},
      m_fragments{std::move(fragments)} {}

const HtmlConfig &HtmlService::config() const { return m_config; }

const HtmlResourceLocator &HtmlService::resource_locator() const {
  return m_resource_locator;
}

const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
HtmlService::fragments() const {
  return m_fragments;
}

HtmlFragment::HtmlFragment(std::string name, HtmlConfig config,
                           HtmlResourceLocator resource_locator)
    : m_name{name}, m_config{config}, m_resource_locator{resource_locator} {}

std::string HtmlFragment::name() const { return m_name; }

const HtmlConfig &HtmlFragment::config() const { return m_config; }

const HtmlResourceLocator &HtmlFragment::resource_locator() const {
  return m_resource_locator;
}

odr::HtmlResource HtmlResource::create(HtmlResourceType type,
                                       const std::string &name,
                                       const std::string &path,
                                       const odr::File &file, bool is_shipped,
                                       bool is_relocatable) {
  return odr::HtmlResource(std::make_shared<HtmlResource>(
      type, name, path, file, is_shipped, is_relocatable));
}

HtmlResource::HtmlResource(HtmlResourceType type, const std::string &name,
                           const std::string &path, const odr::File &file,
                           bool is_shipped, bool is_relocatable)
    : m_type{type}, m_name{name}, m_path{path}, m_file{file},
      m_is_shipped{is_shipped}, m_is_relocatable{is_relocatable} {}

HtmlResourceType HtmlResource::type() const { return m_type; }

const std::string &HtmlResource::name() const { return m_name; }

const std::string &HtmlResource::path() const { return m_path; }

const odr::File &HtmlResource::file() const { return m_file; }

bool HtmlResource::is_shipped() const { return m_is_shipped; }

bool HtmlResource::is_relocatable() const { return m_is_relocatable; }

void HtmlResource::write_resource(std::ostream &os) const {
  util::stream::pipe(*m_file.stream(), os);
}

} // namespace odr::internal::common
