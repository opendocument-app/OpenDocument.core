#ifndef ODR_ACCESS_STREAM_UTIL_H
#define ODR_ACCESS_STREAM_UTIL_H

#include <iostream>
#include <string>

namespace odr::access::StreamUtil {
std::string read(std::istream &);

void pipe(std::istream &, std::ostream &);
} // namespace odr::access::StreamUtil

#endif // ODR_ACCESS_STREAM_UTIL_H
