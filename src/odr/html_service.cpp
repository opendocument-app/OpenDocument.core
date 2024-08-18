#include <odr/html_service.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <iostream>

namespace odr {

HtmlService::HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl)
    : m_impl{std::move(impl)} {}

std::vector<HtmlFragment> HtmlService::fragments() const {
  std::vector<HtmlFragment> result;
  for (const auto &fragment : m_impl->fragments()) {
    result.emplace_back(fragment);
  }
  return result;
}

HtmlFragment::HtmlFragment(
    std::shared_ptr<internal::abstract::HtmlFragment> impl)
    : m_impl{std::move(impl)} {}

std::string HtmlFragment::name() const { return m_impl->name(); }

void HtmlFragment::write_html_fragment(
    std::ostream &os, const odr::HtmlConfig &config,
    const odr::HtmlResourceLocator &resourceLocator) const {
  internal::html::HtmlWriter out(os, config);

  m_impl->write_html_fragment(out, config, resourceLocator);
}

} // namespace odr
