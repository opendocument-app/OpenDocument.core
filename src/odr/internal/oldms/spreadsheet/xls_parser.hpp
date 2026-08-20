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

/// Whether the workbook is encrypted, i.e. whether the globals substream
/// carries a FilePass record ([MS-XLS] 2.4.117). Record headers stay in the
/// clear, which is what makes this readable at all. A `/Workbook` stream that
/// is missing or malformed reads as not encrypted — parsing then fails on its
/// own.
[[nodiscard]] bool
password_encrypted(const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::spreadsheet
