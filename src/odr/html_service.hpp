#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlDocumentService;
class HtmlFragmentService;
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

class HtmlDocumentService final {
public:
  HtmlDocumentService();
  explicit HtmlDocumentService(
      std::shared_ptr<internal::abstract::HtmlDocumentService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const;

  HtmlResources write_document(std::ostream &os) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlDocumentService> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlDocumentService> m_impl;
};

class HtmlFragmentService final {
public:
  explicit HtmlFragmentService(
      std::shared_ptr<internal::abstract::HtmlFragmentService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const;

  void write_fragment(std::ostream &os, HtmlResources &resources) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlFragmentService> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlFragmentService> m_impl;
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
