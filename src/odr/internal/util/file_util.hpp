#pragma once

#include <string>

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::util::file {

std::size_t size(const std::string &path);

std::string read(const std::string &path);

void write(const std::string &path, const std::string &data);

void write(const std::string &path, const abstract::File &file);

} // namespace odr::internal::util::file
