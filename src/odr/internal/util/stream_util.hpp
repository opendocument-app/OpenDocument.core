#ifndef ODR_INTERNAL_UTIL_STREAM_HPP
#define ODR_INTERNAL_UTIL_STREAM_HPP

#include <iosfwd>
#include <string>

namespace odr::internal::util::stream {

std::string read(std::istream &in);

void pipe(std::istream &in, std::ostream &out);

std::istream &pipe_line(std::istream &in, std::ostream &out);
std::string read_line(std::istream &in);

std::istream &pipe_until(std::istream &in, std::ostream &out, char until_char,
                         bool inclusive);
std::string read_until(std::istream &in, char until_char, bool inclusive);

} // namespace odr::internal::util::stream

#endif // ODR_INTERNAL_UTIL_STREAM_HPP
