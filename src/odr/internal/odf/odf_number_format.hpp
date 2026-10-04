#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <pugixml.hpp>

namespace odr::internal::odf {

/// Looks a data style up by its `style:name`; null where there is none.
using DataStyleLookup = std::function<pugi::xml_node(std::string_view)>;

/// Converts an ODS data style and its maps to a format code, matching
/// LibreOffice export; null for unrepresentable parts such as eras.
[[nodiscard]] std::optional<std::string>
format_code(pugi::xml_node data_style, const DataStyleLookup &lookup);

/// The BCP 47 tag of the data style's `number:language`, `number:script` and
/// `number:country`, or of its `number:rfc-language-tag`.
[[nodiscard]] std::optional<std::string>
data_style_locale(pugi::xml_node data_style);

} // namespace odr::internal::odf
