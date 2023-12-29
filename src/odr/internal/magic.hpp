#ifndef ODR_MAGIC_HPP
#define ODR_MAGIC_HPP

#include <string>

namespace odr {
enum class FileType;
}

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::magic {
FileType file_type(const std::string &magic);
FileType file_type(const internal::abstract::File &file);
} // namespace odr::internal::magic

#endif // ODR_MAGIC_HPP
