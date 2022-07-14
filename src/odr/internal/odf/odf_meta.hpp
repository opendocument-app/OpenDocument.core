#ifndef ODR_INTERNAL_ODF_META_H
#define ODR_INTERNAL_ODF_META_H

#include <cstdint>
#include <exception>
#include <odr/internal/common/path.h>
#include <string>
#include <unordered_map>

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

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_META_H
