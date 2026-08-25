#include <odr/internal/markdown/markdown_style.hpp>

#include <array>
#include <stdexcept>
#include <string_view>

namespace odr::internal::markdown {

namespace {

/// The browsers' default heading scale, in `em` so it composes with whatever
/// font size the viewer sets.
constexpr std::array heading_font_sizes{2.0, 1.5, 1.17, 1.0, 0.83, 0.67};

/// One `<blockquote>` worth of indent, per nesting level.
constexpr double quote_margin = 2.5;

/// A generic family rather than a face: nothing in a markdown file names one,
/// and the viewer's monospace font is the closest thing to an author's intent.
/// Static storage, so `TextStyle::font_name` may point at it.
constexpr std::string_view monospace_font_name = "monospace";

constexpr std::uint32_t default_style_index = 0;
constexpr std::uint32_t first_heading_style_index = 1;
/// Derived, so a seventh heading size moves these rather than landing on one.
constexpr std::uint32_t monospace_style_index =
    first_heading_style_index +
    static_cast<std::uint32_t>(heading_font_sizes.size());
constexpr std::uint32_t emphasis_style_index = monospace_style_index + 1;
constexpr std::uint32_t strong_style_index = emphasis_style_index + 1;
constexpr std::uint32_t strikethrough_style_index = strong_style_index + 1;

} // namespace

StyleRegistry::StyleRegistry() {
  m_text_styles.resize(strikethrough_style_index + 1);

  for (std::size_t i = 0; i < heading_font_sizes.size(); ++i) {
    TextStyle &style = m_text_styles.at(first_heading_style_index + i);
    style.font_size = Measure(heading_font_sizes.at(i), DynamicUnit("em"));
    style.font_weight = FontWeight::bold;
  }

  m_text_styles.at(monospace_style_index).font_name = monospace_font_name;
  m_text_styles.at(emphasis_style_index).font_style = FontStyle::italic;
  m_text_styles.at(strong_style_index).font_weight = FontWeight::bold;
  m_text_styles.at(strikethrough_style_index).font_line_through = true;

  m_paragraph_styles.resize(default_style_index + 1);
}

const TextStyle &StyleRegistry::text_style(const std::uint32_t index) const {
  return m_text_styles.at(index);
}

const ParagraphStyle &
StyleRegistry::paragraph_style(const std::uint32_t index) const {
  return m_paragraph_styles.at(index);
}

std::uint32_t StyleRegistry::heading_style(const std::uint32_t level) const {
  if (level < 1 || level > heading_font_sizes.size()) {
    throw std::out_of_range("markdown: heading level out of range");
  }
  return first_heading_style_index + level - 1;
}

std::uint32_t StyleRegistry::monospace_style() const {
  return monospace_style_index;
}

std::uint32_t StyleRegistry::emphasis_style() const {
  return emphasis_style_index;
}

std::uint32_t StyleRegistry::strong_style() const { return strong_style_index; }

std::uint32_t StyleRegistry::strikethrough_style() const {
  return strikethrough_style_index;
}

std::uint32_t StyleRegistry::quote_style(const std::uint32_t depth) {
  if (depth == 0) {
    return default_style_index;
  }
  while (m_paragraph_styles.size() <= depth) {
    ParagraphStyle &style = m_paragraph_styles.emplace_back();
    style.margin.left = Measure(quote_margin * (m_paragraph_styles.size() - 1),
                                DynamicUnit("em"));
  }
  return depth;
}

} // namespace odr::internal::markdown
