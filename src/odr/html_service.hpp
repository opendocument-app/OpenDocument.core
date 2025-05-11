#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlView;
class HtmlResource;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
class File;
struct HtmlConfig;

class Html;
class HtmlView;
class HtmlResource;

enum class HtmlResourceType {
  html_fragment,
  css,
  js,
  image,
  font,
};

using HtmlViews = std::vector<HtmlView>;
using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator = std::function<HtmlResourceLocation(
    const HtmlResource &resource, const HtmlConfig &config)>;
using HtmlResources =
    std::vector<std::pair<HtmlResource, HtmlResourceLocation>>;

class HtmlService final {
public:
  HtmlService();
  explicit HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlViews &list_views() const;

  void warmup() const;

  [[nodiscard]] bool exists(const std::string &path) const;
  [[nodiscard]] std::string mimetype(const std::string &path) const;

  void write(const std::string &path, std::ostream &out) const;
  HtmlResources write_html(const std::string &path, std::ostream &out) const;

  [[nodiscard]] Html bring_offline(const std::string &output_path) const;
  [[nodiscard]] Html bring_offline(const std::string &output_path,
                                   const std::vector<HtmlView> &views) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlService> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlService> m_impl;
};

class HtmlView final {
public:
  HtmlView();
  explicit HtmlView(std::shared_ptr<internal::abstract::HtmlView> impl);

  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const HtmlConfig &config() const;

  HtmlResources write_html(std::ostream &out) const;

  [[nodiscard]] Html bring_offline(const std::string &output_path) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlView> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlView> m_impl;
};

class HtmlResource final {
public:
  HtmlResource();
  explicit HtmlResource(std::shared_ptr<internal::abstract::HtmlResource> impl);

  [[nodiscard]] HtmlResourceType type() const;
  [[nodiscard]] const std::string &mime_type() const;
  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const std::optional<File> &file() const;
  [[nodiscard]] bool is_shipped() const;
  [[nodiscard]] bool is_external() const;
  [[nodiscard]] bool is_accessible() const;

  void write_resource(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlResource> m_impl;
};

} // namespace odr
