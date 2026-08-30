#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::svg {

/// @ref util::number::to_string_significant at six digits, and `0` where
/// @p value is not finite - `nan` in an attribute drops the element.
[[nodiscard]] std::string format_number(double value);

/// @brief Writes escaped svg markup.
///
/// The `style` attribute is accumulated as declarations arrive and written when
/// the element takes content or ends, so attributes and style may be written in
/// any order.
class SvgWriter final {
public:
  explicit SvgWriter(std::ostream &out);

  void write_element_begin(std::string_view name);
  /// Closes the innermost open element, as `/>` where it took no content.
  void write_element_end();

  void write_attribute(std::string_view name, std::string_view value);
  void write_attribute(std::string_view name, double value);
  /// One declaration of the element's `style` attribute; a `;` in @p value is
  /// dropped rather than allowed to open another.
  void write_style(std::string_view property, std::string_view value);
  void write_style(std::string_view property, double value);

  void write_text(std::string_view text);

private:
  /// Ends the open tag, whether it takes content or not.
  void close_tag(bool with_content);

  std::ostream *m_out{nullptr};
  std::vector<std::string> m_stack;
  bool m_tag_open{false};
  /// The open tag's `style` declarations, unwritten.
  std::string m_style;
};

} // namespace odr::internal::svg
