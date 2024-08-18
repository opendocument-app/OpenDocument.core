#ifndef ODR_HTML_SERVICE_HPP
#define ODR_HTML_SERVICE_HPP

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlFragment;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
class File;
struct HtmlConfig;

class HtmlFragment;

enum class HtmlResourceType {
  css,
  js,
  image,
};

using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator = std::function<HtmlResourceLocation(
    HtmlResourceType type, const std::string &name, const std::string &path,
    const File &resource, bool is_core_resource)>;

class HtmlService final {
public:
  explicit HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl);

  [[nodiscard]] std::vector<HtmlFragment> fragments() const;

private:
  std::shared_ptr<internal::abstract::HtmlService> m_impl;
};

class HtmlFragment final {
public:
  explicit HtmlFragment(std::shared_ptr<internal::abstract::HtmlFragment> impl);

  [[nodiscard]] std::string name() const;

  void write_html_fragment(std::ostream &os, const HtmlConfig &config,
                           const HtmlResourceLocator &resourceLocator) const;

private:
  std::shared_ptr<internal::abstract::HtmlFragment> m_impl;
};

} // namespace odr

#endif // ODR_HTML_SERVICE_HPP
