#pragma once

#include <iosfwd>
#include <string>

namespace odr::internal::util::stream {

std::string read(std::istream &in);
std::string read(std::istream &in, std::size_t size);

void pipe(std::istream &in, std::ostream &out);

std::istream &pipe_line(std::istream &in, std::ostream &out, bool inclusive);
std::string read_line(std::istream &in, bool inclusive);

std::istream &pipe_until(std::istream &in, std::ostream &out, char until_char,
                         bool inclusive);
std::string read_until(std::istream &in, char until_char, bool inclusive);

} // namespace odr::internal::util::stream
