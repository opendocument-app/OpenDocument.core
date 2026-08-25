#pragma once

#include <odr/style.hpp>

#include <cstdint>
#include <vector>

namespace odr::internal::markdown {

/// Owns the document's styles, indexed by the style index stored on elements;
/// 0 is the default style in both sets. Markdown carries no style information
/// of its own, so the set is a fixed rendering convention: a heading scale, a
/// monospace face for code, the three emphasis marks, and one left margin per
/// block-quote nesting depth.
class StyleRegistry final {
public:
  StyleRegistry();

  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;
  [[nodiscard]] const ParagraphStyle &
  paragraph_style(std::uint32_t index) const;

  /// @throws std::out_of_range unless @p level is 1 to 6.
  [[nodiscard]] std::uint32_t heading_style(std::uint32_t level) const;
  [[nodiscard]] std::uint32_t monospace_style() const;
  [[nodiscard]] std::uint32_t emphasis_style() const;
  [[nodiscard]] std::uint32_t strong_style() const;
  [[nodiscard]] std::uint32_t strikethrough_style() const;

  /// The style of a paragraph nested in @p depth block quotes, interned on
  /// first use; the default style for depth 0.
  std::uint32_t quote_style(std::uint32_t depth);

private:
  std::vector<TextStyle> m_text_styles;
  std::vector<ParagraphStyle> m_paragraph_styles;
};

} // namespace odr::internal::markdown
