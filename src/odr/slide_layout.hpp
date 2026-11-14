#pragma once

#include <odr/quantity.hpp>

#include <string>
#include <vector>

namespace odr {

/// What a text placeholder of a slide layout holds.
enum class SlidePlaceholderKind {
  title,
  subtitle,
  body,
};

/// A text placeholder of a slide layout, positioned on the page.
class SlidePlaceholder final {
public:
  SlidePlaceholder(SlidePlaceholderKind kind, const Measure &x,
                   const Measure &y, const Measure &width,
                   const Measure &height);

  [[nodiscard]] SlidePlaceholderKind kind() const noexcept;
  [[nodiscard]] const Measure &x() const noexcept;
  [[nodiscard]] const Measure &y() const noexcept;
  [[nodiscard]] const Measure &width() const noexcept;
  [[nodiscard]] const Measure &height() const noexcept;

private:
  SlidePlaceholderKind m_kind;
  Measure m_x;
  Measure m_y;
  Measure m_width;
  Measure m_height;
};

/// A layout a slide can start from: its name, and the text placeholders a
/// slide made from it holds, in document order.
class SlideLayout final {
public:
  SlideLayout(std::string name, std::vector<SlidePlaceholder> placeholders);

  [[nodiscard]] const std::string &name() const noexcept;
  [[nodiscard]] const std::vector<SlidePlaceholder> &
  placeholders() const noexcept;

private:
  std::string m_name;
  std::vector<SlidePlaceholder> m_placeholders;
};

} // namespace odr
