#pragma once

#include <odr/html_service.hpp>

#include <iosfwd>
#include <memory>

namespace odr::internal::html {
class HtmlWriter;
}

namespace odr::internal::abstract {
class File;
class HtmlFragment;

class HtmlService {
public:
  virtual ~HtmlService() = default;

  [[nodiscard]] virtual const std::vector<std::shared_ptr<HtmlFragment>> &
  fragments() const = 0;

  virtual void
  write_html_document(html::HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const = 0;
};

class HtmlFragment {
public:
  virtual ~HtmlFragment() = default;

  [[nodiscard]] virtual std::string name() const = 0;

  virtual void
  write_html_fragment(html::HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const = 0;

  virtual void
  write_html_document(html::HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const = 0;
};

} // namespace odr::internal::abstract
