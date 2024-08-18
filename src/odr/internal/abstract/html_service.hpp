#ifndef ODR_INTERNAL_ABSTRACT_HTML_SERVICE_HPP
#define ODR_INTERNAL_ABSTRACT_HTML_SERVICE_HPP

#include <odr/html_service.hpp>

#include <iosfwd>
#include <memory>

namespace odr::internal::abstract {
class File;
class HtmlFragment;

class HtmlService {
public:
  virtual ~HtmlService() = default;

  [[nodiscard]] virtual const std::vector<std::shared_ptr<HtmlFragment>> &
  fragments() const = 0;
};

class HtmlFragment {
public:
  virtual ~HtmlFragment() = default;

  [[nodiscard]] virtual std::string name() const = 0;

  virtual void
  write_html_fragment(std::ostream &os, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_HTML_SERVICE_HPP
