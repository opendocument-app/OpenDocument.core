#pragma once

#include <odr/style.hpp>

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::text {

/// Owns the document's resolved character styles, indexed by the style index
/// stored on paragraph/span elements, 0 being the default style. Immutable
/// after construction.
class StyleRegistry final {
public:
  StyleRegistry() = default;
  /// `styles` are the resolved character styles, index 0 the default style.
  explicit StyleRegistry(std::vector<TextStyle> styles);

  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;

private:
  std::vector<TextStyle> m_styles;
};

/// The base character style Chpx grpprls apply on top of; without sprmCHps
/// the font size defaults to 20 half-points ([MS-DOC] 2.6.1 sprmCHps).
[[nodiscard]] TextStyle default_character_style();

/// Applies the character SPRMs of a Chpx grpprl ([MS-DOC] 2.6.1) on top of
/// `style`; non-character SPRMs are skipped via their operand size.
/// `font_names` resolves sprmCRgFtc0.
TextStyle apply_character_sprms(TextStyle style, std::string_view grpprl,
                                std::span<const std::string> font_names);

/// Reads the font names (FFN.xszFfn) of the SttbfFfn ([MS-DOC] 2.9.286) at
/// `fc` in the table stream; empty for lcb == 0.
std::vector<std::string> read_font_names(std::istream &table_stream,
                                         FcLcb sttbf_ffn);

} // namespace odr::internal::oldms::text
