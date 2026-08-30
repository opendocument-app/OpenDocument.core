#pragma once

#include <optional>
#include <string>

#include <pugixml.hpp>

namespace odr::internal::odf {

/// The `<office:chart>` an embedded object's `content.xml` holds (12), drawn
/// to svg. Nothing where the part carries no chart we can read.
[[nodiscard]] std::optional<std::string>
render_chart(pugi::xml_node content_root);

} // namespace odr::internal::odf
