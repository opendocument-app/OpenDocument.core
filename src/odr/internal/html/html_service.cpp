#include <odr/internal/html/html_service.hpp>

#include <odr/internal/util/stream_util.hpp>

#include <odr/exceptions.hpp>

#include <iostream>

namespace odr::internal::html {

HtmlService::HtmlService(HtmlConfig config) : m_config{std::move(config)} {}

const HtmlConfig &HtmlService::config() const { return m_config; }

HtmlView::HtmlView(const abstract::HtmlService &service, std::string name,
                   std::string path)
    : m_service{&service}, m_name{std::move(name)}, m_path{std::move(path)} {}

const std::string &HtmlView::name() const { return m_name; }

const std::string &HtmlView::path() const { return m_path; }

const HtmlConfig &HtmlView::config() const { return m_service->config(); }

const abstract::HtmlService &HtmlView::service() const { return *m_service; }

HtmlResources HtmlView::write_html(html::HtmlWriter &out) const {
  return m_service->write_html(path(), out);
}

odr::HtmlResource HtmlResource::create(HtmlResourceType type,
                                       std::string mime_type, std::string name,
                                       std::string path,
                                       std::optional<File> file,
                                       bool is_shipped, bool is_external,
                                       bool is_accessible) {
  return odr::HtmlResource(std::make_shared<HtmlResource>(
      type, std::move(mime_type), std::move(name), std::move(path),
      std::move(file), is_shipped, is_external, is_accessible));
}

HtmlResource::HtmlResource(HtmlResourceType type, std::string mime_type,
                           std::string name, std::string path,
                           std::optional<File> file, bool is_shipped,
                           bool is_external, bool is_accessible)
    : m_type{type}, m_mime_type{std::move(mime_type)}, m_name{std::move(name)},
      m_path{std::move(path)}, m_file{std::move(file)},
      m_is_shipped{is_shipped}, m_is_external{is_external},
      m_is_accessible{is_accessible} {}

HtmlResourceType HtmlResource::type() const { return m_type; }

const std::string &HtmlResource::mime_type() const { return m_mime_type; }

const std::string &HtmlResource::name() const { return m_name; }

const std::string &HtmlResource::path() const { return m_path; }

const std::optional<File> &HtmlResource::file() const { return m_file; }

bool HtmlResource::is_shipped() const { return m_is_shipped; }

bool HtmlResource::is_external() const { return m_is_external; }

bool HtmlResource::is_accessible() const { return m_is_accessible; }

void HtmlResource::write_resource(std::ostream &os) const {
  if (!is_accessible() || !file().has_value()) {
    throw ResourceNotAccessible(name(), path());
  }

  util::stream::pipe(*file()->stream(), os);
}

} // namespace odr::internal::html
