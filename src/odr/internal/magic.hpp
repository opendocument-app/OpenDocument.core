#pragma once

#include <iosfwd>
#include <string>

namespace odr {
enum class FileType;
class File;
} // namespace odr

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::magic {
FileType file_type(const std::string &magic);
FileType file_type(std::istream &in);
FileType file_type(const internal::abstract::File &file);
FileType file_type(const File &file);
} // namespace odr::internal::magic
