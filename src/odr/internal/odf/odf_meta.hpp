#pragma once

namespace pugi {
class xml_document;
class xml_node;
} // namespace pugi

namespace odr {
struct FileMeta;
} // namespace odr

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                         const pugi::xml_document *manifest, bool decrypted);

/// Reads the meta of a flat xml document off its `office:document` root.
/// @throws NoOpenDocumentFile if @p root is not one we decode.
FileMeta parse_flat_file_meta(pugi::xml_node root);

} // namespace odr::internal::odf
