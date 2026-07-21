#pragma once

#include <odr/style.hpp>

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::text {

/// Owns the document's resolved character styles — indexed by the style index
/// stored on paragraph/span elements, 0 being the default style — and the
/// font names `TextStyle::font_name` points into.
class StyleRegistry final {
public:
  /// Stores a font name and returns a pointer that stays valid for the
  /// registry's lifetime (`TextStyle::font_name` is a `const char *`).
  const char *intern_font_name(const std::string &name);

  std::uint32_t add_style(TextStyle style);
  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;

private:
  /// Deque: elements never move, so interned `c_str()`s stay valid.
  std::deque<std::string> m_font_names;
  std::vector<TextStyle> m_styles;
};

/// Applies the character SPRMs of a Chpx grpprl ([MS-DOC] 2.6.1) on top of
/// `style`; non-character SPRMs are skipped via their operand size.
/// `font_names` resolves sprmCRgFtc0 (pointers must outlive the style).
TextStyle apply_character_sprms(TextStyle style, std::string_view grpprl,
                                const std::vector<const char *> &font_names);

/// Reads the font names (FFN.xszFfn) of the SttbfFfn ([MS-DOC] 2.9.286) at
/// `fc` in the table stream; empty for lcb == 0.
std::vector<std::string> read_font_names(std::istream &table_stream,
                                         FcLcb sttbf_ffn);

} // namespace odr::internal::oldms::text
