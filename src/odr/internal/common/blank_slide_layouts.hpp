#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace odr::internal {

/// What a placeholder of a new presentation holds.
enum class BlankPlaceholderKind {
  title,
  centered_title, ///< the title of a title slide
  subtitle,
  content,
  text,
};

/// A placeholder of a new presentation, in hundredths of a centimetre.
struct BlankPlaceholder final {
  BlankPlaceholderKind kind{BlankPlaceholderKind::title};
  std::int32_t x{0};
  std::int32_t y{0};
  std::int32_t width{0};
  std::int32_t height{0};
};

/// A slide layout of a new odp or pptx; see
/// `docs/design/structure-editing.md`.
struct BlankSlideLayout final {
  std::string_view name;
  std::string_view ooxml_type; ///< [ECMA-376] 19.7.15 `ST_SlideLayoutType`
  std::span<const BlankPlaceholder> placeholders;
};

/// The 16:9 page LibreOffice sets, in hundredths of a centimetre.
constexpr std::int32_t blank_slide_width = 2800;
constexpr std::int32_t blank_slide_height = 1575;

constexpr BlankPlaceholder blank_title{BlankPlaceholderKind::title, 140, 63,
                                       2520, 263};
constexpr BlankPlaceholder blank_content{BlankPlaceholderKind::content, 140,
                                         369, 2520, 913};

constexpr std::array blank_title_slide_placeholders{
    BlankPlaceholder{BlankPlaceholderKind::centered_title, 210, 258, 2380, 548},
    BlankPlaceholder{BlankPlaceholderKind::subtitle, 350, 827, 2100, 380},
};
constexpr std::array blank_title_content_placeholders{blank_title,
                                                      blank_content};
constexpr std::array blank_section_header_placeholders{
    BlankPlaceholder{BlankPlaceholderKind::title, 191, 393, 2415, 656},
    BlankPlaceholder{BlankPlaceholderKind::text, 191, 1055, 2415, 345},
};
constexpr std::array blank_two_content_placeholders{
    blank_title,
    BlankPlaceholder{BlankPlaceholderKind::content, 140, 369, 1230, 913},
    BlankPlaceholder{BlankPlaceholderKind::content, 1430, 369, 1230, 913},
};
constexpr std::array blank_title_only_placeholders{blank_title};

/// The master's own placeholders, which a layout's title and body follow.
constexpr std::array blank_master_placeholders{blank_title, blank_content};

constexpr std::array blank_slide_layouts{
    BlankSlideLayout{"Title Slide", "title", blank_title_slide_placeholders},
    BlankSlideLayout{"Title and Content", "obj",
                     blank_title_content_placeholders},
    BlankSlideLayout{"Section Header", "secHead",
                     blank_section_header_placeholders},
    BlankSlideLayout{"Two Content", "twoObj", blank_two_content_placeholders},
    BlankSlideLayout{"Title Only", "titleOnly", blank_title_only_placeholders},
    BlankSlideLayout{"Blank", "blank", {}},
};

} // namespace odr::internal
