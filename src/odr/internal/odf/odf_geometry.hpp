#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <pugixml.hpp>

namespace odr {
struct DrawingTransform;
struct DrawingPath;
} // namespace odr

namespace odr::internal::odf {

/// `draw:transform` (19.228) off @p node, its operation list composed into one
/// transform. Nothing where the attribute is absent or unreadable.
[[nodiscard]] std::optional<DrawingTransform>
read_transform(pugi::xml_node node);

/// Angles are radians; the list applies left to right.
[[nodiscard]] std::optional<DrawingTransform>
parse_transform(std::string_view value);

/// A length attribute in 1/100 mm, the unit ODF measures a chart and an
/// `svg:d` with no view box in. Nothing where it is absent or not a length.
[[nodiscard]] std::optional<double>
read_hundredth_millimetres(pugi::xml_attribute attribute);

/// The outline @p node draws: `draw:path`, `draw:polygon`, `draw:polyline`,
/// `draw:regular-polygon`, `draw:connector`, and a `draw:circle`/`draw:ellipse`
/// that `draw:kind` cuts. Nothing for a shape with no geometry we can read.
[[nodiscard]] std::optional<DrawingPath> read_path(pugi::xml_node node);

/// An svg `d` (19.180), read and written back out, boxed by every point and
/// control point in it. Nothing where it does not parse.
[[nodiscard]] std::optional<DrawingPath> parse_path_data(std::string_view data);

} // namespace odr::internal::odf
