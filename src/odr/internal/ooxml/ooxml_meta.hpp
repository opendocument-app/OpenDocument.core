#pragma once

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
