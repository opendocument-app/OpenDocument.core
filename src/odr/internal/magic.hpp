#pragma once

#include <iosfwd>
#include <string>

namespace odr {
enum class FileType;
class File;
class Logger;
} // namespace odr

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::magic {
FileType file_type(const std::string &magic);
FileType file_type(std::istream &in);
FileType file_type(const abstract::File &file);
FileType file_type(const File &file);

/// Opens the file to tell what a container holds, so it needs a logger like the
/// rest of the open strategy.
std::string_view mimetype(const std::string &path, const Logger &logger);
} // namespace odr::internal::magic
