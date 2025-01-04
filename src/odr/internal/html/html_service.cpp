#include "html_service.hpp"

#include "odr/internal/util/stream_util.hpp"

#include <iostream>

namespace odr::internal::common {

odr::HtmlResource
StaticHtmlResource::create(HtmlResourceType type, const std::string &name,
                           const std::string &path, const odr::File &file,
                           bool is_shipped, bool is_relocatable) {
  return odr::HtmlResource(std::make_shared<StaticHtmlResource>(
      type, name, path, file, is_shipped, is_relocatable));
}

StaticHtmlResource::StaticHtmlResource(HtmlResourceType type,
                                       const std::string &name,
                                       const std::string &path,
                                       const odr::File &file, bool is_shipped,
                                       bool is_relocatable)
    : m_type{type}, m_name{name}, m_path{path}, m_file{file},
      m_is_shipped{is_shipped}, m_is_relocatable{is_relocatable} {}

HtmlResourceType StaticHtmlResource::type() const { return m_type; }

const std::string &StaticHtmlResource::name() const { return m_name; }

const std::string &StaticHtmlResource::path() const { return m_path; }

const odr::File &StaticHtmlResource::file() const { return m_file; }

bool StaticHtmlResource::is_shipped() const { return m_is_shipped; }

bool StaticHtmlResource::is_relocatable() const { return m_is_relocatable; }

void StaticHtmlResource::write_resource(std::ostream &os) const {
  util::stream::pipe(*m_file.stream(), os);
}

} // namespace odr::internal::common
