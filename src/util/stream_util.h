#ifndef ODR_UTIL_STREAM_H
#define ODR_UTIL_STREAM_H

#include <iostream>
#include <string>

namespace odr::util::stream {
std::string read(std::istream &);

void pipe(std::istream &, std::ostream &);
} // namespace odr::util::stream

#endif // ODR_UTIL_STREAM_H
