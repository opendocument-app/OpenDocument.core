#pragma once

#include <iosfwd>
#include <string>

namespace odr::internal::util::file {

std::ifstream open(const std::string &path);
std::size_t size(const std::string &path);
std::string read(const std::string &path);
void pipe(const std::string &path, std::ostream &out);

std::ofstream create(const std::string &path);
void write(const std::string &data, const std::string &path);
void write(std::istream &in, const std::string &path);

} // namespace odr::internal::util::file
