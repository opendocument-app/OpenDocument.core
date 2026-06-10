#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms::spreadsheet {

class ElementRegistry;

/// Parses the `/Workbook` BIFF8 stream into root → sheet → cell → paragraph →
/// text elements. \return the root element id.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::spreadsheet
