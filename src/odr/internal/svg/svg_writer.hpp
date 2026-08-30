#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::svg {

/// Escapes @p text for element content, and drops the control characters xml
/// 1.0 does not allow. Not `html::escape_text`: that one emits `&nbsp;`, which
/// no xml parser knows.
[[nodiscard]] std::string escape_text(std::string_view text);
/// As @ref escape_text, plus the quote that would end an attribute value.
[[nodiscard]] std::string escape_attribute(std::string_view value);

/// @p value at the six significant digits a stream defaults to, but in the
/// classic locale and never in exponent notation - css reads neither `1,5` nor
/// `1.2e+3`.
[[nodiscard]] std::string format_number(double value);

/// @brief Writes svg markup.
///
/// Elements nest through @ref write_element_begin / @ref write_element_end;
/// attributes, style declarations and text go to the innermost open one.
/// Everything written is escaped, and numbers are formatted independently of
/// the stream's locale and precision.
///
/// An element's `style` attribute is accumulated as declarations arrive and
/// written when the element takes content or ends, so attributes and style may
/// be written in any order.
class SvgWriter final {
public:
  explicit SvgWriter(std::ostream &out);

  /// Opens `<name`. What follows belongs to it until @ref write_element_end.
  void write_element_begin(std::string_view name);
  /// Closes the innermost open element, as `/>` where it took no content.
  void write_element_end();

  void write_attribute(std::string_view name, std::string_view value);
  void write_attribute(std::string_view name, double value);
  /// One declaration of the element's `style` attribute.
  void write_style(std::string_view property, std::string_view value);
  void write_style(std::string_view property, double value);

  void write_text(std::string_view text);

private:
  /// Ends the open tag, whether it takes content or not.
  void close_tag(bool with_content);

  std::ostream *m_out{nullptr};
  std::vector<std::string> m_stack;
  /// The innermost element's tag is still open for attributes.
  bool m_tag_open{false};
  /// The open tag's `style` declarations, unwritten.
  std::string m_style;
};

} // namespace odr::internal::svg
