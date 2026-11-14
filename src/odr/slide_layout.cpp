#include <odr/slide_layout.hpp>

#include <utility>

namespace odr {

SlidePlaceholder::SlidePlaceholder(const SlidePlaceholderKind kind, Measure x,
                                   Measure y, Measure width, Measure height)
    : m_kind{kind}, m_x{std::move(x)}, m_y{std::move(y)},
      m_width{std::move(width)}, m_height{std::move(height)} {}

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
