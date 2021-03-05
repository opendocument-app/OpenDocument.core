#ifndef ODR_ODF_META_H
#define ODR_ODF_META_H

#include <common/path.h>
#include <cstdint>
#include <exception>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
class xml_node;
} // namespace pugi

namespace odr {
struct FileMeta;
struct DocumentMeta;
} // namespace odr

namespace odr::abstract {
class ReadableFilesystem;
} // namespace odr::abstract

namespace odr::odf {

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                         const pugi::xml_document *manifest);

void estimate_table_dimensions(const pugi::xml_node &table, std::uint32_t &rows,
                               std::uint32_t &cols);

} // namespace odr::odf

#endif // ODR_ODF_META_H
