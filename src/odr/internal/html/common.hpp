#ifndef ODR_INTERNAL_HTML_COMMON_HPP
#define ODR_INTERNAL_HTML_COMMON_HPP

#include <iosfwd>
#include <string>

#include <odr/html_service.hpp>
#include <odr/internal/abstract/html_service.hpp>

namespace odr {
struct Color;
struct HtmlConfig;
class Html;
} // namespace odr

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::html {

std::string escape_text(std::string text);

std::string color(const Color &color);

std::string file_to_url(const std::string &file, const std::string &mimeType);
std::string file_to_url(std::istream &file, const std::string &mimeType);
std::string file_to_url(const abstract::File &file,
                        const std::string &mimeType);

HtmlResourceLocator local_resource_locator(const std::string &output_path,
                                           const HtmlConfig &config);

class StaticHtmlService : public abstract::HtmlService {
public:
  explicit StaticHtmlService(
      std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments)
      : m_fragments(std::move(fragments)) {}

  [[nodiscard]] const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
  fragments() const override {
    return m_fragments;
  }

private:
  const std::vector<std::shared_ptr<abstract::HtmlFragment>> m_fragments;
};

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_COMMON_HPP
