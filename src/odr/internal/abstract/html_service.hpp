#pragma once

#include <odr/html_service.hpp>

#include <iosfwd>
#include <memory>
#include <optional>

namespace odr {
class File;
}

namespace odr::internal::html {
class HtmlWriter;
}

namespace odr::internal::abstract {

class HtmlService {
public:
  virtual ~HtmlService() = default;

  [[nodiscard]] virtual const HtmlConfig &config() const = 0;
  [[nodiscard]] virtual const HtmlResourceLocator &resource_locator() const = 0;
  [[nodiscard]] virtual const HtmlViews &list_views() const = 0;

  virtual void warmup() const = 0;

  [[nodiscard]] virtual bool exists(const std::string &path) const = 0;
  [[nodiscard]] virtual std::string mimetype(const std::string &path) const = 0;

  virtual void write(const std::string &path, std::ostream &out) const = 0;
  virtual HtmlResources write_html(const std::string &path,
                                   html::HtmlWriter &out) const = 0;
};

class HtmlView {
public:
  virtual ~HtmlView() = default;

  [[nodiscard]] virtual const std::string &name() const = 0;
  [[nodiscard]] virtual const std::string &path() const = 0;
  [[nodiscard]] virtual const HtmlConfig &config() const = 0;

  virtual HtmlResources write_html(html::HtmlWriter &out) const = 0;
};

class HtmlResource {
public:
  virtual ~HtmlResource() = default;

  [[nodiscard]] virtual HtmlResourceType type() const = 0;
  [[nodiscard]] virtual const std::string &mime_type() const = 0;
  [[nodiscard]] virtual const std::string &name() const = 0;
  [[nodiscard]] virtual const std::string &path() const = 0;
  [[nodiscard]] virtual const std::optional<odr::File> &file() const = 0;
  [[nodiscard]] virtual bool is_shipped() const = 0;
  [[nodiscard]] virtual bool is_external() const = 0;
  [[nodiscard]] virtual bool is_accessible() const = 0;

  virtual void write_resource(std::ostream &os) const = 0;
};

} // namespace odr::internal::abstract
