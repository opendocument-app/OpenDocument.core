#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms::spreadsheet {

class ElementRegistry;
class StyleRegistry;

/// Parses the `/Workbook` BIFF8 stream into root → sheet → cell → paragraph →
/// text elements; fills `style_registry` from the globals substream.
/// \return the root element id.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             StyleRegistry &style_registry,
                             const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::spreadsheet
