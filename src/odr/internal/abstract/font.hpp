#pragma once

#include <odr/font.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::abstract {

/// Read-only view over a font program: counts, metrics, names and character
/// maps. Outlines are never decompiled, the glyph bytes pass through untouched.
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

  /// The font's own character map: code point -> glyph id, 0 (`.notdef`) when
  /// unmapped.
  [[nodiscard]] virtual std::uint16_t
  glyph_for_code_point(char32_t code_point) const = 0;

  /// The reverse of that map, `nullopt` when no code point reaches @p glyph.
  /// Recovers Unicode for a font without usable `/ToUnicode` or `/Encoding`.
  [[nodiscard]] virtual std::optional<char32_t>
  code_point_for_glyph(std::uint16_t glyph) const = 0;

  /// The glyph a PostScript glyph name selects, 0 when the font names none —
  /// including every format that carries no glyph names (ISO 32000-1 9.6.6.2).
  [[nodiscard]] virtual std::uint16_t glyph_for_name(std::string_view) const {
    return 0;
  }
};

} // namespace odr::internal::abstract
