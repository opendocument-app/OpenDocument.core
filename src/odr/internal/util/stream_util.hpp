#ifndef ODR_INTERNAL_UTIL_STREAM_HPP
#define ODR_INTERNAL_UTIL_STREAM_HPP

#include <iosfwd>
#include <string>

namespace odr::internal::util::stream {

std::string read(std::istream &in);

void pipe(std::istream &in, std::ostream &out);

std::istream &getline(std::istream &in, std::ostream &out);

} // namespace odr::internal::util::stream

#endif // ODR_INTERNAL_UTIL_STREAM_HPP
