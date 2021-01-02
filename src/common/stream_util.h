#ifndef ODR_COMMON_STREAM_UTIL_H
#define ODR_COMMON_STREAM_UTIL_H

#include <iostream>
#include <string>

namespace odr::common::StreamUtil {
std::string read(std::istream &);

void pipe(std::istream &, std::ostream &);
} // namespace odr::common::StreamUtil

#endif // ODR_COMMON_STREAM_UTIL_H
