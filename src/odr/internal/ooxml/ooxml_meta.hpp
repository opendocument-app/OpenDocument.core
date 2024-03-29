#ifndef ODR_INTERNAL_OOXML_META_HPP
#define ODR_INTERNAL_OOXML_META_HPP

#include <memory>
#include <stdexcept>

namespace odr {
struct FileMeta;
}

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem);

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_META_HPP
