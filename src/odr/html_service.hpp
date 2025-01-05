#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlFragment;
class HtmlResource;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
class File;
struct HtmlConfig;

class HtmlFragment;
class HtmlResource;

enum class HtmlResourceType {
  css,
  js,
  image,
};

using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator =
    std::function<HtmlResourceLocation(const HtmlResource &resource)>;
using HtmlResources =
    std::vector<std::pair<HtmlResource, HtmlResourceLocation>>;

class HtmlService final {
public:
  explicit HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const;

  [[nodiscard]] std::vector<HtmlFragment> fragments() const;

  HtmlResources write_document(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlService> m_impl;
};

class HtmlFragment final {
public:
  explicit HtmlFragment(std::shared_ptr<internal::abstract::HtmlFragment> impl);

  [[nodiscard]] std::string name() const;

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const;

  void write_fragment(std::ostream &os, HtmlResources &resources) const;

  HtmlResources write_document(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlFragment> m_impl;
};

class HtmlResource final {
public:
  HtmlResource();
  explicit HtmlResource(std::shared_ptr<internal::abstract::HtmlResource> impl);

  [[nodiscard]] HtmlResourceType type() const;
  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const File &file() const;
  [[nodiscard]] bool is_shipped() const;
  [[nodiscard]] bool is_relocatable() const;

  void write_resource(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlResource> m_impl;
};

} // namespace odr
