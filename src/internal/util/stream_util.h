#ifndef ODR_INTERNAL_UTIL_STREAM_H
#define ODR_INTERNAL_UTIL_STREAM_H

#include <iostream>
#include <string>

namespace odr::internal::util::stream {
std::string read(std::istream &);

void pipe(std::istream &, std::ostream &);
} // namespace odr::internal::util::stream

#endif // ODR_INTERNAL_UTIL_STREAM_H
