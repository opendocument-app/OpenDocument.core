#pragma once

#include <optional>
#include <string_view>

#include <pugixml.hpp>

namespace odr {
struct DrawingTransform;
}

namespace odr::internal::odf {

/// `draw:transform` (19.228) off @p node, its operation list composed into one
/// transform. Nothing where the attribute is absent or unreadable.
[[nodiscard]] std::optional<DrawingTransform>
read_transform(pugi::xml_node node);

/// Angles are radians; the list applies left to right.
[[nodiscard]] std::optional<DrawingTransform>
parse_transform(std::string_view value);

} // namespace odr::internal::odf
