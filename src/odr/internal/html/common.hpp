#pragma once

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

struct WritingState {
  WritingState(HtmlWriter &out, const HtmlConfig &config,
               HtmlResources &resources)
      : m_out{&out}, m_config{&config}, m_resources(&resources) {}

  [[nodiscard]] HtmlWriter &out() const { return *m_out; }
  [[nodiscard]] const HtmlConfig &config() const { return *m_config; }
  [[nodiscard]] HtmlResources &resources() const { return *m_resources; }

private:
  HtmlWriter *m_out;
  const HtmlConfig *m_config;
  HtmlResources *m_resources;
};

std::string escape_text(std::string text);

std::string color(const Color &color);

std::string file_to_url(const std::string &file, const std::string &mime_type);
std::string file_to_url(std::istream &file, const std::string &mime_type);
std::string file_to_url(const abstract::File &file,
                        const std::string &mime_type);

} // namespace odr::internal::html
