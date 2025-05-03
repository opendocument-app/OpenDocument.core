#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlResource;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
class File;
struct HtmlConfig;

class HtmlResource;

enum class HtmlResourceType {
  html_fragment,
  css,
  js,
  image,
  font,
};

using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator =
    std::function<HtmlResourceLocation(const HtmlResource &resource)>;
using HtmlResources =
    std::vector<std::pair<HtmlResource, HtmlResourceLocation>>;

class HtmlService final {
public:
  HtmlService();
  explicit HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const;

  void warmup() const;

  [[nodiscard]] std::string mimetype(const std::string &path) const;

  void write(const std::string &path, std::ostream &out) const;
  HtmlResources write_html(const std::string &path, std::ostream &out) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlService> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlService> m_impl;
};

class HtmlResource final {
public:
  HtmlResource();
  explicit HtmlResource(std::shared_ptr<internal::abstract::HtmlResource> impl);

  [[nodiscard]] HtmlResourceType type() const;
  [[nodiscard]] const std::string &mime_type() const;
  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const File &file() const;
  [[nodiscard]] bool is_shipped() const;
  [[nodiscard]] bool is_relocatable() const;
  [[nodiscard]] bool is_external() const;

  void write_resource(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlResource> m_impl;
};

} // namespace odr
