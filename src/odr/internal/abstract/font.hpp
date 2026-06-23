#pragma once

#include <odr/font.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace odr::internal::abstract {

/// @brief Read-only view over a font program, exposing the *facts* every
/// consumer needs while the raw glyph bytes pass through untouched.
///
/// Per the "IR for facts, pass-through for glyphs" architecture this never
/// decompiles outlines: it reports counts / metrics / names and hands back the
/// original bytes. The embedded-font reverse map reads Unicode from it, the OTF
/// wrap synthesizes the SFNT skeleton from it, and the PUA re-encoder assigns
/// code points from its glyph count.
class Font {
public:
  virtual ~Font() = default;

  [[nodiscard]] virtual FontFormat format() const noexcept = 0;

  /// PostScript name (`name` ID 6) when available, else the full font name
  /// (ID 4); empty when the font names nothing.
  [[nodiscard]] virtual std::string name() const = 0;

  /// Number of glyphs; valid glyph ids are `[0, glyph_count())`.
  [[nodiscard]] virtual std::uint16_t glyph_count() const noexcept = 0;

  /// Font design units per em (typically 1000 or 2048).
  [[nodiscard]] virtual std::uint16_t units_per_em() const noexcept = 0;

  /// Whether the font declares a symbolic (non-standard) character set.
  [[nodiscard]] virtual bool symbolic() const noexcept = 0;

  [[nodiscard]] virtual FontBBox bounding_box() const noexcept = 0;

  /// Advance width of @p glyph in design units, 0 when unknown.
  [[nodiscard]] virtual std::uint16_t
  advance_width(std::uint16_t glyph) const = 0;

  /// The font's own forward map: Unicode code point -> glyph id, 0 (`.notdef`)
  /// when unmapped. Seeds the specimen page and the PUA re-encode.
  [[nodiscard]] virtual std::uint16_t
  glyph_for_code_point(char32_t code_point) const = 0;

  /// The reverse map: glyph id -> Unicode code point, when the font's character
  /// map reaches the glyph. This is the embedded-font reverse map that closes
  /// the stage-1 extraction gap (3.3); `nullopt` when no code point maps.
  [[nodiscard]] virtual std::optional<char32_t>
  code_point_for_glyph(std::uint16_t glyph) const = 0;
};

} // namespace odr::internal::abstract
