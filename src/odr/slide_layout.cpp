#include <odr/slide_layout.hpp>

#include <utility>

namespace odr {

SlidePlaceholder::SlidePlaceholder(const SlidePlaceholderKind kind,
                                   const Measure &x, const Measure &y,
                                   const Measure &width, const Measure &height)
    : m_kind{kind}, m_x{x}, m_y{y}, m_width{width}, m_height{height} {}

SlidePlaceholderKind SlidePlaceholder::kind() const noexcept { return m_kind; }

const Measure &SlidePlaceholder::x() const noexcept { return m_x; }

const Measure &SlidePlaceholder::y() const noexcept { return m_y; }

const Measure &SlidePlaceholder::width() const noexcept { return m_width; }

const Measure &SlidePlaceholder::height() const noexcept { return m_height; }

SlideLayout::SlideLayout(std::string name,
                         std::vector<SlidePlaceholder> placeholders)
    : m_name{std::move(name)}, m_placeholders{std::move(placeholders)} {}

const std::string &SlideLayout::name() const noexcept { return m_name; }

const std::vector<SlidePlaceholder> &
SlideLayout::placeholders() const noexcept {
  return m_placeholders;
}

} // namespace odr
