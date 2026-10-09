#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace odr::internal::font::cff {

class CffFont;

/// Wraps CFF bytes in an OpenType font with synthesized metrics and a PUA cmap
/// plus @p extra mappings. Throws if the font has no glyph beyond `.notdef`.
[[nodiscard]] std::string
wrap_to_otf(const CffFont &font,
            const std::map<char32_t, std::uint16_t> &extra = {});

} // namespace odr::internal::font::cff
