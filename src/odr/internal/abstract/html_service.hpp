#pragma once

#include <odr/html_service.hpp>

#include <iosfwd>
#include <memory>

namespace odr {
class File;
}

namespace odr::internal::html {
class HtmlWriter;
}

namespace odr::internal::abstract {

class HtmlDocumentService {
public:
  virtual ~HtmlDocumentService() = default;

  [[nodiscard]] virtual const HtmlConfig &config() const = 0;
  [[nodiscard]] virtual const HtmlResourceLocator &resource_locator() const = 0;

  virtual HtmlResources write_document(html::HtmlWriter &out) const = 0;
};

class HtmlFragmentService {
public:
  virtual ~HtmlFragmentService() = default;

  [[nodiscard]] virtual const HtmlConfig &config() const = 0;
  [[nodiscard]] virtual const HtmlResourceLocator &resource_locator() const = 0;

  virtual void write_fragment(html::HtmlWriter &out,
                              HtmlResources &resources) const = 0;
};

class HtmlResource {
public:
  virtual ~HtmlResource() = default;

  [[nodiscard]] virtual HtmlResourceType type() const = 0;
  [[nodiscard]] virtual const std::string &mime_type() const = 0;
  [[nodiscard]] virtual const std::string &name() const = 0;
  [[nodiscard]] virtual const std::string &path() const = 0;
  [[nodiscard]] virtual const odr::File &file() const = 0;
  [[nodiscard]] virtual bool is_shipped() const = 0;
  [[nodiscard]] virtual bool is_relocatable() const = 0;
  [[nodiscard]] virtual bool is_external() const = 0;

  virtual void write_resource(std::ostream &os) const = 0;
};

} // namespace odr::internal::abstract
