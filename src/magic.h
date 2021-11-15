#ifndef ODR_MAGIC_H
#define ODR_MAGIC_H

#include <string>

namespace odr {
enum class FileType;
}

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::magic {
FileType file_type(const std::string &magic);
FileType file_type(const internal::abstract::File &file);
} // namespace odr::magic

#endif // ODR_MAGIC_H
