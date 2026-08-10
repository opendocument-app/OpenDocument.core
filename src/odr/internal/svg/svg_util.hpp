#pragma once

#include <cstddef>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::xml {
class XmlFile;
} // namespace odr::internal::xml

namespace odr::internal::svg {

/// Throws @ref odr::NoSvgFile unless @p file's root element is `svg`.
void check_svg_file(const xml::XmlFile &file);

/// Renames every element under @p node whose prefix binds the svg namespace to
/// its local name. The html parser enters foreign content on `svg`, not on
/// `s:svg`, so a document that binds the namespace to a prefix would otherwise
/// not draw at all.
void drop_namespace_prefixes(pugi::xml_node node);

/// Strips from @p node everything that would run or fetch once the markup is
/// in a page: script elements, event handlers, references that are neither
/// same-document nor an embedded image, and css that reaches outside the
/// document. Returns the number of nodes and attributes removed.
std::size_t sanitize(pugi::xml_node node);

} // namespace odr::internal::svg
