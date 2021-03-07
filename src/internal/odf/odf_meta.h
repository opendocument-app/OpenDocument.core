#ifndef ODR_INTERNAL_ODF_META_H
#define ODR_INTERNAL_ODF_META_H

#include <cstdint>
#include <exception>
#include <internal/common/path.h>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
class xml_node;
} // namespace pugi

namespace odr::experimental {
struct FileMeta;
} // namespace odr::experimental

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

experimental::FileMeta
parse_file_meta(const abstract::ReadableFilesystem &filesystem,
                const pugi::xml_document *manifest);

void estimate_table_dimensions(const pugi::xml_node &table, std::uint32_t &rows,
                               std::uint32_t &cols);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_META_H
