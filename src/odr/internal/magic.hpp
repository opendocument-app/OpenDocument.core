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
FileType file_type(const abstract::File &file);
FileType file_type(const File &file);

const char *mime_type(const std::string &path);
} // namespace odr::internal::magic
